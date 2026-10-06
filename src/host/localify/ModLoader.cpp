#include "ModLoader.hpp"
#include "GakumasLocalify/Log.h"
#include "imgui/imgui.h"
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <algorithm>

namespace GakumasLocal::ModLoader {

namespace {

std::vector<LoadedPlugin> g_plugins;
std::mutex g_mutex;
bool g_initialized = false;
bool g_unityReady = false;

std::wstring g_gameRootW;
std::wstring g_localRootW;
std::wstring g_pluginsRootW;
GakumasModContext g_modContext{};

void HostLogInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    GakumasLocal::Log::InfoFmt("%s", buf);
}

void HostLogWarn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    GakumasLocal::Log::InfoFmt("[WARN] %s", buf);
}

void HostLogError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    GakumasLocal::Log::ErrorFmt("%s", buf);
}

bool QueryVRActive() {
    // VRify checks can be wired here or default to false
    return false;
}

bool QueryVRifyLiveGazeActive() {
    return false;
}

} // namespace

void Initialize(const std::filesystem::path& gameRoot, const std::filesystem::path& localRoot) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return;

    g_gameRootW = gameRoot.wstring();
    g_localRootW = localRoot.wstring();
    const auto pluginsPath = localRoot / "plugins";
    g_pluginsRootW = pluginsPath.wstring();

    std::error_code ec;
    if (!std::filesystem::exists(pluginsPath, ec)) {
        std::filesystem::create_directories(pluginsPath, ec);
    }

    g_modContext.apiVersion = GAKUMAS_MOD_API_VERSION;
    g_modContext.gameRoot = g_gameRootW.c_str();
    g_modContext.localRoot = g_localRootW.c_str();
    g_modContext.pluginsRoot = g_pluginsRootW.c_str();
    g_modContext.gameAssemblyHandle = GetModuleHandleW(L"GameAssembly.dll");
    g_modContext.logInfo = HostLogInfo;
    g_modContext.logWarn = HostLogWarn;
    g_modContext.logError = HostLogError;
    g_modContext.isVRActive = QueryVRActive;
    g_modContext.isVRifyLiveGazeActive = QueryVRifyLiveGazeActive;

    GakumasLocal::Log::InfoFmt("[ModLoader] Scanning plugins directory: %ls", g_pluginsRootW.c_str());

    for (const auto& entry : std::filesystem::directory_iterator(pluginsPath, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto path = entry.path();
        if (path.extension() != ".dll") continue;

        const auto filename = path.filename().string();
        if (filename.empty() || filename[0] == '.') continue;

        GakumasLocal::Log::InfoFmt("[ModLoader] Probing native plugin: %s", filename.c_str());
        HMODULE hMod = LoadLibraryW(path.c_str());
        if (!hMod) {
            GakumasLocal::Log::ErrorFmt("[ModLoader] Failed to load library: %s (Error %lu)", filename.c_str(), GetLastError());
            continue;
        }

        auto initFn = reinterpret_cast<FnGakumasPlugin_Init>(GetProcAddress(hMod, "GakumasPlugin_Init"));
        auto getInfoFn = reinterpret_cast<FnGakumasPlugin_GetInfo>(GetProcAddress(hMod, "GakumasPlugin_GetInfo"));

        if (!initFn && !getInfoFn) {
            // Not a Gakumas native mod plugin, unload
            FreeLibrary(hMod);
            continue;
        }

        LoadedPlugin plugin;
        plugin.dllPath = path.wstring();
        plugin.handle = hMod;
        plugin.initFn = initFn;
        plugin.onUnityReadyFn = reinterpret_cast<FnGakumasPlugin_OnUnityReady>(GetProcAddress(hMod, "GakumasPlugin_OnUnityReady"));
        plugin.onLateUpdateFn = reinterpret_cast<FnGakumasPlugin_OnLateUpdate>(GetProcAddress(hMod, "GakumasPlugin_OnLateUpdate"));
        plugin.onRenderGuiFn = reinterpret_cast<FnGakumasPlugin_OnRenderGui>(GetProcAddress(hMod, "GakumasPlugin_OnRenderGui"));
        plugin.shutdownFn = reinterpret_cast<FnGakumasPlugin_Shutdown>(GetProcAddress(hMod, "GakumasPlugin_Shutdown"));

        if (getInfoFn) {
            getInfoFn(&plugin.info);
        } else {
            plugin.info.name = filename.c_str();
            plugin.info.version = "1.0.0";
            plugin.info.author = "Unknown";
            plugin.info.description = "Native Plugin";
        }

        if (plugin.initFn) {
            if (!plugin.initFn(&g_modContext)) {
                GakumasLocal::Log::ErrorFmt("[ModLoader] Plugin %s initialization returned false; unloading", plugin.info.name);
                FreeLibrary(hMod);
                continue;
            }
        }

        GakumasLocal::Log::InfoFmt("[ModLoader] Successfully loaded plugin: %s v%s by %s (%s)",
            plugin.info.name, plugin.info.version, plugin.info.author, plugin.info.description);

        g_plugins.push_back(plugin);
    }

    g_initialized = true;
    GakumasLocal::Log::InfoFmt("[ModLoader] Initialized. Total loaded plugins: %zu", g_plugins.size());
}

void DispatchUnityReady() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || g_unityReady) return;
    g_unityReady = true;

    for (auto& plugin : g_plugins) {
        if (plugin.onUnityReadyFn) {
            plugin.onUnityReadyFn();
        }
    }
}

void DispatchLateUpdate(void* activeActor, float deltaTime) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return;

    for (auto& plugin : g_plugins) {
        if (plugin.onLateUpdateFn) {
            plugin.onLateUpdateFn(activeActor, deltaTime);
        }
    }
}

void DispatchRenderGui() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return;

    for (auto& plugin : g_plugins) {
        if (plugin.onRenderGuiFn) {
            plugin.onRenderGuiFn();
        }
    }
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return;

    for (auto& plugin : g_plugins) {
        if (plugin.shutdownFn) {
            plugin.shutdownFn();
        }
        if (plugin.handle) {
            FreeLibrary(plugin.handle);
            plugin.handle = nullptr;
        }
    }
    g_plugins.clear();
    g_initialized = false;
    g_unityReady = false;
}

const std::vector<LoadedPlugin>& GetLoadedPlugins() {
    return g_plugins;
}

} // namespace GakumasLocal::ModLoader

extern "C" __declspec(dllexport) void* GakumasVr_GetImGuiContext() {
    return ImGui::GetCurrentContext();
}
