#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include "GakumasModPlugin.h"

namespace GakumasLocal::ModLoader {

struct LoadedPlugin {
    std::wstring dllPath;
    HMODULE handle{nullptr};
    GakumasPluginInfo info{};

    FnGakumasPlugin_Init initFn{nullptr};
    FnGakumasPlugin_OnUnityReady onUnityReadyFn{nullptr};
    FnGakumasPlugin_OnLateUpdate onLateUpdateFn{nullptr};
    FnGakumasPlugin_OnRenderGui onRenderGuiFn{nullptr};
    FnGakumasPlugin_Shutdown shutdownFn{nullptr};
};

void Initialize(const std::filesystem::path& gameRoot, const std::filesystem::path& localRoot);
void DispatchUnityReady();
void DispatchLateUpdate(void* activeActor, float deltaTime);
void DispatchRenderGui();
void Shutdown();

const std::vector<LoadedPlugin>& GetLoadedPlugins();

} // namespace GakumasLocal::ModLoader

extern "C" __declspec(dllexport) void* GakumasVr_GetImGuiContext();
