// A native runner linked against the actual GakuLoader host core. Only the
// desktop UI/dialog shell is replaced; service registry, lifecycle, actor entry
// and owner-bound MinHook leases are the production implementations.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Host.hpp"
#include "ActorEntry.hpp"
#include "DesktopUi.hpp"
#include "DesktopDialogs.hpp"
#include "host/GakuVrifyDiagnostics.h"
#include "host/GakuVrifyService.h"
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace GakuLoader::DesktopUi {
bool Initialize() { return true; }
void Shutdown() {}
const GakuDesktopUiService* GetService() { return nullptr; }
const GakuDesktopWindowService* GetWindowService() { return nullptr; }
const GakuAppearanceService* GetAppearanceService() { return nullptr; }
GakuStatus SetFileDropTarget(const char*, const GakuFileDropTarget*) { return GAKU_E_UNAVAILABLE; }
GakuStatus PollFileDrop(const char*, uint32_t, GakuFileDropResult*, uint32_t, char*) {
    return GAKU_E_UNAVAILABLE;
}
GakuStatus RequestShowPanel(const char*) { return GAKU_E_UNAVAILABLE; }
void ToggleManager() {}
bool WantsTextInput() { return false; }
}
namespace GakuLoader::DesktopDialogs {
GakuFileDialogService ServiceForOwner(void*) { return {}; }
void CancelOwner(void*) {}
}

namespace {
using CounterFn = uint64_t (*)(uint32_t);
using TargetFn = void* (*)(uint32_t);
struct Evidence {
    std::wstring scenario;
    std::vector<std::string> events, modules, queried;
    GakuVrifySnapshot before{}, after{};
    std::array<uint64_t, 7> metadata{}, backend{};
    std::array<uint64_t, 9> cameraProvider{};
    uint32_t patched = 0, restored = 0;
    uint64_t shutdownMs = 0;
    size_t actorBefore = 0, actorAfter = 0;
    bool activated = false, runtimeReady = false;
    bool unavailableInHostLog = false;
    bool coexist = false;
    std::string error;
};
Evidence evidence;
std::filesystem::path testRoot;
void Check(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
std::string Utf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}
std::string Quote(std::string_view value) {
    std::string result = "\"";
    for (const unsigned char c : value) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c < 32) { char text[8]; std::snprintf(text, sizeof(text), "\\u%04x", c); result += text; }
        else result.push_back(c);
    }
    return result + '"';
}
void Event(const char* name) {
    evidence.events.emplace_back(name);
    std::printf("VRLESS_EVENT %s\n", name);
    std::fflush(stdout);
}
std::vector<std::string> Modules() {
    std::vector<std::string> result;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    Check(snapshot != INVALID_HANDLE_VALUE, "cannot enumerate own process modules");
    MODULEENTRY32W item{}; item.dwSize = sizeof(item);
    if (Module32FirstW(snapshot, &item)) do { result.push_back(Utf8(item.szModule)); } while (Module32NextW(snapshot, &item));
    CloseHandle(snapshot);
    return result;
}
void AssertNoLocalify() {
    evidence.modules = Modules();
    for (auto name : evidence.modules) {
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name.find("localify") != std::string::npos) {
            const auto module = GetModuleHandleW(L"GakuLocalify.dll");
            Check(evidence.coexist && name == "gakulocalify.dll" && module &&
                GetProcAddress(module, "GakuVrifyTestCameraProviderCounter"),
                "unexpected external Localify module loaded");
        }
        Check(name.find("vuplex") == std::string::npos, "unexpected Vuplex/bootstrap module loaded");
        Check(name != "version_vrify.dll", "legacy VRify proxy loaded");
    }
    Check(!std::filesystem::exists(testRoot / "gakumas-local/config.json") &&
        !std::filesystem::exists(testRoot / "gakumas-local/localizationConfig.json"),
        "Localify business configuration was created");
}
void SnapshotJson(std::ostream& out, const GakuVrifySnapshot& s) {
    out << "{\"lifecyclePhase\":" << s.lifecyclePhase << ",\"lifecycleName\":" << Quote(s.lifecycleName ? s.lifecycleName : "")
        << ",\"runtimeName\":" << Quote(s.runtimeName ? s.runtimeName : "")
        << ",\"reasonName\":" << Quote(s.reasonName ? s.reasonName : "")
        << ",\"unavailableReason\":" << s.unavailableReason << ",\"lastOpenXrResult\":" << s.lastOpenXrResult
        << ",\"unityInitialized\":" << s.unityInitialized << ",\"actorBindings\":" << s.actorBindings
        << ",\"graphicsHooksReady\":" << s.graphicsHooksReady << ",\"backendAttempts\":" << s.backendAttempts
        << ",\"initializationStatus\":" << s.initializationStatus << '}';
}
void Report() {
    std::ofstream out(testRoot / "result.json", std::ios::binary | std::ios::trunc);
    out << "{\"scenario\":" << Quote(Utf8(evidence.scenario)) << ",\"status\":" << Quote(evidence.error.empty() ? "PASS" : "FAIL")
        << ",\"error\":" << Quote(evidence.error) << ",\"hostImplementation\":\"production GakuLoader core; desktop shell stubbed\""
        << ",\"activated\":" << (evidence.activated ? "true" : "false")
        << ",\"runtimeReadyReturned\":" << (evidence.runtimeReady ? "true" : "false")
        << ",\"unavailableInHostLog\":" << (evidence.unavailableInHostLog ? "true" : "false")
        << ",\"shutdownMs\":" << evidence.shutdownMs << ",\"actorParticipantsBeforeShutdown\":" << evidence.actorBefore
        << ",\"actorParticipantsAfterShutdown\":" << evidence.actorAfter
        << ",\"criticalTargetsPatched\":" << evidence.patched << ",\"criticalTargetsRestored\":" << evidence.restored;
    out << ",\"beforeShutdown\":"; SnapshotJson(out, evidence.before);
    out << ",\"afterShutdown\":"; SnapshotJson(out, evidence.after);
    const auto stringArray = [&](const char* name, const std::vector<std::string>& values) {
        out << "," << Quote(name) << ":[";
        for (size_t i = 0; i < values.size(); ++i) { if (i) out << ','; out << Quote(values[i]); }
        out << ']';
    };
    stringArray("events", evidence.events); stringArray("modules", evidence.modules); stringArray("servicesVerified", evidence.queried);
    out << ",\"metadataCounters\":[";
    for (size_t i = 0; i < evidence.metadata.size(); ++i) { if (i) out << ','; out << evidence.metadata[i]; }
    out << "],\"backendCounters\":[";
    for (size_t i = 0; i < evidence.backend.size(); ++i) { if (i) out << ','; out << evidence.backend[i]; }
    out << "],\"cameraProviderCounters\":[";
    for (size_t i = 0; i < evidence.cameraProvider.size(); ++i) { if (i) out << ','; out << evidence.cameraProvider[i]; }
    out << "],\"cameraProviderFixture\":" << (evidence.coexist ? "true" : "false") << "}\n";
}
template<class T> T Service(const GakuLoader::LoadedPlugin& plugin, const char* name) {
    T result{};
    Check(plugin.host.queryService(plugin.host.hostContext, name, GAKU_SERVICE_VERSION,
        sizeof(result), &result) == GAKU_OK, "actual host service query failed");
    evidence.queried.emplace_back(name);
    return result;
}
}

int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        Check(argc == 3, "usage: vrless-smoke <scenario> <isolated-game-root>");
        evidence.scenario = argv[1]; testRoot = std::filesystem::absolute(argv[2]);
        Check(SetCurrentDirectoryW(testRoot.c_str()) != FALSE, "cannot isolate process working directory");
        evidence.coexist = evidence.scenario.starts_with(L"coexist-");
        const bool unity = evidence.scenario.starts_with(L"unity-") || evidence.coexist;
        const bool loaderMissing = evidence.scenario.ends_with(L"loader-missing");
        const bool deviceMissing = evidence.scenario.ends_with(L"device-missing");
        const bool invalidLoader = evidence.scenario.ends_with(L"invalid-loader");
        const bool metadataMissing = evidence.scenario.ends_with(L"metadata-missing");
        const bool targetMismatch = evidence.scenario.ends_with(L"target-mismatch");
        const bool realBackend = evidence.scenario.find(L"real-runtime-missing") != std::wstring::npos;
        SetEnvironmentVariableW(L"GAKU_VRLESS_OMIT_CRITICAL", metadataMissing ? L"1" : nullptr);
        SetEnvironmentVariableW(L"GAKU_VRLESS_CAMERA_TARGET_MISMATCH", targetMismatch ? L"1" : nullptr);
        AssertNoLocalify();
        Check(GetModuleHandleW(L"GameAssembly.dll") == nullptr, "unexpected game assembly at activation");
        const auto local = testRoot / "gakumas-local";
        GakuLoader::Initialize(testRoot, local);
        Check(GakuLoader::BindMainThread(GetCurrentThreadId()), "cannot bind host main thread");
        GakuLoader::ActivatePlugins();
        const auto& plugins = GakuLoader::GetLoadedPlugins();
        Check(plugins.size() == (evidence.coexist ? 2u : 1u), "wrong actual host plugin count");
        const GakuLoader::LoadedPlugin* vrifyPlugin = nullptr;
        CounterFn providerCounter = nullptr;
        for (const auto& candidate : plugins) {
            Check(candidate.activated && !candidate.failed, "production host failed a plugin activation");
            if (candidate.id == "gaku.vrify") vrifyPlugin = &candidate;
            else {
                Check(evidence.coexist && candidate.id == "fixture.localify-camera-render", "unexpected plugin activated");
                providerCounter = reinterpret_cast<CounterFn>(GetProcAddress(candidate.handle, "GakuVrifyTestCameraProviderCounter"));
            }
        }
        Check(vrifyPlugin && (evidence.coexist == (providerCounter != nullptr)), "production VRify/provider activation evidence missing");
        const auto& plugin = *vrifyPlugin; evidence.activated = true;
        Check(plugin.handle != nullptr && GetProcAddress(plugin.handle, "GakuPlugin_Query"), "production DLL was not independently loaded");
        const auto log = Service<GakuLogService>(plugin, GAKU_SERVICE_LOG);
        const auto runtime = Service<GakuRuntimeService>(plugin, GAKU_SERVICE_RUNTIME);
        const auto hooks = Service<GakuHookService>(plugin, GAKU_SERVICE_HOOKS);
        const auto actors = Service<GakuActorEntryService>(plugin, GAKU_SERVICE_ACTOR_ENTRY);
        Check(log.write && runtime.getGameAssembly && hooks.acquireBatch && actors.registerEntry, "actual host returned incomplete required capabilities");
        const auto vr = Service<GakuVrifyService>(plugin, GAKU_VRIFY_SERVICE);
        const auto diagnostics = Service<GakuVrifyDiagnosticsService>(plugin, GAKU_VRIFY_DIAGNOSTICS_SERVICE);
        Check(diagnostics.getSnapshot != nullptr, "missing diagnostic callback");
        GakuVrifySnapshot sentinel{};
        std::memset(&sentinel, 0xA5, sizeof(sentinel));
        auto unchanged = sentinel;
        Check(diagnostics.getSnapshot(sizeof(unchanged) - 1, &unchanged) == GAKU_E_SIZE &&
            std::memcmp(&unchanged, &sentinel, sizeof(sentinel)) == 0,
            "short diagnostics output was accepted or overwritten");
        Check(diagnostics.getSnapshot(sizeof(unchanged), nullptr) == GAKU_E_INVALID_ARGUMENT,
            "null diagnostics output was accepted");
        Event(evidence.coexist ? "real-host-activated-vrify-and-camera-provider-fixture" :
            "independent-dll-activation-without-game-or-localify");

        HMODULE metadataModule = nullptr;
        CounterFn metadataCounter = nullptr; TargetFn target = nullptr;
        std::array<void*, 4> targets{};
        std::array<std::array<unsigned char, 16>, 4> originalBytes{};
        if (unity) {
            metadataModule = LoadLibraryExW((testRoot / "GameAssembly.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            Check(metadataModule != nullptr, "could not load explicit IL2CPP metadata fixture");
            metadataCounter = reinterpret_cast<CounterFn>(GetProcAddress(metadataModule, "GakuVrifyTestMetadataCounter"));
            target = reinterpret_cast<TargetFn>(GetProcAddress(metadataModule, "GakuVrifyTestMetadataHookTarget"));
            Check(metadataCounter && target, "metadata fixture missing evidence exports");
            for (uint32_t i = 0; i < 4; ++i) {
                targets[i] = target(i); Check(targets[i] != nullptr, "metadata target missing");
                std::memcpy(originalBytes[i].data(), targets[i], originalBytes[i].size());
            }
            Event("metadata-fixture-loaded-after-activation-no-localify-bootstrap");
        }
        HMODULE backendModule = nullptr; CounterFn backendCounter = nullptr;
        if (!loaderMissing && !invalidLoader && !metadataMissing && !targetMismatch) {
            if (realBackend) {
                const auto nonexistentRuntime = testRoot / "intentionally-absent-openxr-runtime.json";
                Check(!std::filesystem::exists(nonexistentRuntime), "forced missing runtime unexpectedly exists");
                SetEnvironmentVariableW(L"XR_RUNTIME_JSON", nonexistentRuntime.c_str());
            }
            SetEnvironmentVariableW(L"GAKU_VRIFY_TEST_XR_MODE", deviceMissing ? L"device-unavailable" : L"runtime-unavailable");
            // Hold one explicit harness reference so counters remain callable
            // after production shutdown unloads its own loader reference.
            backendModule = LoadLibraryExW((testRoot / "openxr_loader.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            Check(backendModule != nullptr, "could not load explicit unavailable OpenXR test backend");
            backendCounter = reinterpret_cast<CounterFn>(GetProcAddress(backendModule, "GakuVrifyTestXrCounter"));
            if (realBackend) {
                Check(!backendCounter && GetProcAddress(backendModule, "xrGetInstanceProcAddr"), "expected the real Khronos loader");
                Event("real-khronos-loader-forced-missing-runtime");
            } else Check(backendCounter != nullptr, "test backend missing evidence counter");
        }
        Check(LoadLibraryW(L"d3d11.dll") != nullptr, "D3D11 unavailable in native smoke process");
        GakuLoader::NotifyRuntimeReady(); evidence.runtimeReady = true;
        Event("actual-host-runtime-ready-returned");
        const auto deadline = GetTickCount64() + 12000;
        while (GetTickCount64() < deadline) {
            Check(diagnostics.getSnapshot(sizeof(evidence.before), &evidence.before) == GAKU_OK, "snapshot read failed");
            if (metadataMissing && evidence.before.lifecyclePhase == GAKU_VRIFY_PHASE_FAILED) break;
            if (targetMismatch && evidence.before.lifecyclePhase == GAKU_VRIFY_PHASE_FAILED) break;
            if (invalidLoader && evidence.before.runtimeName && std::strcmp(evidence.before.runtimeName, "Faulted") == 0) break;
            if (evidence.before.lifecyclePhase == GAKU_VRIFY_PHASE_FAILED)
                throw std::runtime_error("plugin failed before the VR runtime/device boundary");
            if (evidence.before.unavailableReason != GAKU_VRIFY_UNAVAILABLE_NONE &&
                evidence.before.runtimeName && std::strcmp(evidence.before.runtimeName, "Unavailable") == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (metadataMissing || targetMismatch) {
            Check(evidence.before.lifecyclePhase == GAKU_VRIFY_PHASE_FAILED &&
                evidence.before.initializationStatus != GAKU_OK && evidence.before.backendAttempts == 0 &&
                evidence.before.unavailableReason == GAKU_VRIFY_UNAVAILABLE_NONE,
                "invalid initialization was confused with VR unavailable");
            Event(targetMismatch ? "negative-control-shared-target-mismatch-rejected-before-backend" :
                "negative-control-unity-initialization-failed-before-backend");
        } else if (invalidLoader) {
            Check(evidence.before.runtimeName && std::strcmp(evidence.before.runtimeName, "Faulted") == 0 &&
                evidence.before.backendAttempts >= 1 && evidence.before.graphicsHooksReady &&
                evidence.before.unavailableReason == GAKU_VRIFY_UNAVAILABLE_NONE,
                "invalid backend DLL was confused with VR unavailable");
            Event("negative-control-invalid-backend-is-a-fault");
        } else {
        Check(evidence.before.lifecyclePhase == GAKU_VRIFY_PHASE_WORKER_STARTED, "XR worker boundary was not reached");
        Check(evidence.before.runtimeName && std::strcmp(evidence.before.runtimeName, "Unavailable") == 0,
            "worker did not report explicit VR unavailable");
        const uint32_t expected = loaderMissing ? GAKU_VRIFY_UNAVAILABLE_LOADER_MISSING :
            (deviceMissing ? GAKU_VRIFY_UNAVAILABLE_HEADSET_MISSING : GAKU_VRIFY_UNAVAILABLE_RUNTIME_MISSING);
        Check(evidence.before.unavailableReason == expected, "wrong unavailable reason");
        {
            const auto logDeadline = GetTickCount64() + 1000;
            do {
            std::ifstream hostLog(GakuLoader::LogPath(), std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(hostLog)), std::istreambuf_iterator<char>());
            evidence.unavailableInHostLog = text.find(std::string("VR unavailable: ") + evidence.before.reasonName) != std::string::npos;
            if (evidence.unavailableInHostLog) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } while (GetTickCount64() < logDeadline);
            Check(evidence.unavailableInHostLog, "VR unavailable was silent in the host log");
        }
        if (!loaderMissing) Check(evidence.before.lastOpenXrResult < 0, "backend unavailable without its actual XR error");
        Check(evidence.before.graphicsHooksReady == 1 && evidence.before.backendAttempts >= 1,
            "plugin stopped before graphics leases and the real backend boundary");
        Check(evidence.before.initializationStatus == GAKU_OK, "non-VR initialization status was a failure");
        Check(vr.isVrActive() == 0 && vr.isLiveGazeActive() == 0, "fake backend advertised a VR session/gaze");
        evidence.actorBefore = GakuLoader::ActorEntry::Count();
        Check(evidence.actorBefore == (unity ? 2u : 0u), "wrong actual host actor participant count");
        Check(evidence.before.unityInitialized == static_cast<uint32_t>(unity), "wrong Unity initialization evidence");
        if (unity) {
            for (uint32_t i = 0; i < 4; ++i)
                if (std::memcmp(originalBytes[i].data(), targets[i], originalBytes[i].size()) != 0) ++evidence.patched;
            Check(evidence.patched == 4, "actual host did not patch all four critical Unity targets");
        }
        Event("explicit-vr-unavailable-after-initialization");
        }
        if (metadataMissing || targetMismatch) Check(GakuLoader::ActorEntry::Count() == 0, "failed initialization left actor callbacks");
        if (providerCounter) {
            Check(providerCounter(0) == 1 && providerCounter(1) == 3 && providerCounter(8) == 0,
                "fixture provider did not acquire its real three leases");
            Check(targetMismatch ? providerCounter(3) == 0 : providerCounter(3) == 1,
                "wrong provider subscription success count");
            std::ifstream hostLog(GakuLoader::LogPath(), std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(hostLog)), std::istreambuf_iterator<char>());
            Check(text.find("VRify hook lease failed") == std::string::npos,
                "VRify attempted a conflicting lease instead of using the shared provider");
            Event("real-provider-owns-three-targets-vrify-did-not-take-over");
        }
        const bool checkWarningDedup = evidence.scenario == L"native-runtime-missing";
        if (checkWarningDedup) {
            const auto retryDeadline = GetTickCount64() + 6000;
            do {
                Check(diagnostics.getSnapshot(sizeof(evidence.before), &evidence.before) == GAKU_OK,
                    "retry snapshot read failed");
                if (evidence.before.backendAttempts >= 3 && evidence.before.runtimeName &&
                    std::strcmp(evidence.before.runtimeName, "Unavailable") == 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } while (GetTickCount64() < retryDeadline);
            Check(evidence.before.backendAttempts >= 3 && evidence.before.runtimeName &&
                std::strcmp(evidence.before.runtimeName, "Unavailable") == 0 &&
                evidence.before.unavailableReason == GAKU_VRIFY_UNAVAILABLE_RUNTIME_MISSING,
                "unavailable warning test did not exercise repeated backend attempts");
        }
        AssertNoLocalify();
        const auto shutdownStarted = GetTickCount64();
        GakuLoader::Shutdown();
        evidence.shutdownMs = GetTickCount64() - shutdownStarted;
        evidence.actorAfter = GakuLoader::ActorEntry::Count();
        Check(evidence.shutdownMs < 4000, "shutdown exceeded bounded smoke budget");
        Check(evidence.actorAfter == 0, "actual host retained actor participants after shutdown");
        Check(diagnostics.getSnapshot(sizeof(evidence.after), &evidence.after) == GAKU_OK, "retained diagnostic service became invalid");
        Check(evidence.after.lifecyclePhase == GAKU_VRIFY_PHASE_STOPPED, "plugin did not report STOPPED");
        Check(vr.isVrActive() == 0 && vr.isLiveGazeActive() == 0, "retained status service remained active after shutdown");
        if (checkWarningDedup) {
            std::ifstream hostLog(GakuLoader::LogPath(), std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(hostLog)), std::istreambuf_iterator<char>());
            const std::string warning = "VR unavailable: runtime-missing";
            size_t count = 0;
            for (size_t at = text.find(warning); at != std::string::npos;
                 at = text.find(warning, at + warning.size())) ++count;
            Check(count == 1, "unchanged unavailable reason produced repeated host warnings");
            Event("runtime-retried-with-one-unavailable-warning");
        }
        if (unity) {
            for (uint32_t i = 0; i < 4; ++i)
                if (std::memcmp(originalBytes[i].data(), targets[i], originalBytes[i].size()) == 0) ++evidence.restored;
            Check(evidence.restored == 4, "actual host did not restore the critical target bytes");
            for (uint32_t i = 0; i < evidence.metadata.size(); ++i) evidence.metadata[i] = metadataCounter(i);
            Check(evidence.metadata[0] > 0 && evidence.metadata[2] > 0 && evidence.metadata[3] > 0 && evidence.metadata[4] > 0,
                "production Unity initializer did not enumerate fixture metadata");
            Check(evidence.metadata[5] == 0 && evidence.metadata[6] == 0, "startup unexpectedly invoked managed/Unity object methods");
        }
        if (backendCounter) {
            for (uint32_t i = 0; i < evidence.backend.size(); ++i) evidence.backend[i] = backendCounter(i);
            Check(evidence.backend[2] > 0 && evidence.backend[6] == 0, "backend not reached or fake admitted unsupported work");
            Check(deviceMissing ? evidence.backend[4] > 0 && evidence.backend[5] > 0 : evidence.backend[4] == 0 && evidence.backend[5] == 0,
                "unexpected system/destroy behavior for the unavailable backend");
        }
        if (providerCounter) {
            for (uint32_t i = 0; i < evidence.cameraProvider.size(); ++i) evidence.cameraProvider[i] = providerCounter(i);
            Check(evidence.cameraProvider[5] == 1, "provider shutdown did not close once");
            if (!targetMismatch) Check(evidence.cameraProvider[4] == 1, "VRify did not unsubscribe its shared callbacks");
            if (targetMismatch) Check(evidence.cameraProvider[7] > 0, "target mismatch negative control was not exercised");
        }
        AssertNoLocalify();
        Event("actual-host-shutdown-completed-leases-restored");
        Report();
        std::printf("PASS: %s; boundary=%s/%s; shutdown=%llums\n", Utf8(evidence.scenario).c_str(),
            evidence.before.lifecycleName, evidence.before.reasonName,
            static_cast<unsigned long long>(evidence.shutdownMs));
        return 0;
    } catch (const std::exception& error) {
        evidence.error = error.what();
        if (!testRoot.empty()) Report();
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
