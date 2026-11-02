#include "GakuPlugin.h"
#include "GakuPaths.h"
#include "GakuVrifyService.h"
#include "GakuVrifyDiagnostics.h"
#include "GakuVrifyUnity.hpp"
#include "CameraBeginProvider.hpp"
#include "HostedHooks.hpp"
#include "vr/VrRuntime.hpp"
#include "vr/VrVersion.hpp"
#include "vr/LiveGaze.hpp"
#include "vr/config/VrifyConfig.hpp"
#include <atomic>
#include <filesystem>
#include <exception>
#include <vector>

extern std::filesystem::path gakumasLocalPath, ProgramConfigJson, ConfigJson, VrConfigJson;
namespace {
GakuLogService logService{};
GakuRuntimeService runtimeService{};
GakuActorEntryService actorService{};
std::filesystem::path vrLogDirectory;
std::atomic<bool> activated{false};
std::atomic<bool> stopping{false};
std::atomic<bool> runtimeNotified{false};
std::atomic<bool> unityReady{false};
std::atomic<bool> actorBeforeRegistered{false};
std::atomic<bool> actorAfterRegistered{false};
std::atomic<uint32_t> lifecyclePhase{GAKU_VRIFY_PHASE_DORMANT};
std::atomic<GakuStatus> initializationStatus{GAKU_OK};

const char* LifecycleName(uint32_t phase) noexcept {
    switch (phase) {
    case GAKU_VRIFY_PHASE_ACTIVATED: return "activated";
    case GAKU_VRIFY_PHASE_RUNTIME_READY: return "runtime-ready";
    case GAKU_VRIFY_PHASE_UNITY_READY: return "unity-ready";
    case GAKU_VRIFY_PHASE_WORKER_STARTED: return "worker-started";
    case GAKU_VRIFY_PHASE_FAILED: return "initialization-failed";
    case GAKU_VRIFY_PHASE_STOPPED: return "stopped";
    default: return "dormant";
    }
}

GakuStatus GetSnapshot(uint32_t size, GakuVrifySnapshot* output) noexcept {
    if (!output) return GAKU_E_INVALID_ARGUMENT;
    if (size < sizeof(*output)) return GAKU_E_SIZE;
    auto& runtime = gakumas::vr::VrRuntime::Instance();
    GakuVrifySnapshot snapshot{};
    snapshot.abiVersion = GAKU_VRIFY_DIAGNOSTICS_VERSION;
    snapshot.structSize = sizeof(snapshot);
    snapshot.lifecyclePhase = lifecyclePhase.load();
    const auto state = runtime.State();
    const auto reason = runtime.UnavailableReason();
    snapshot.runtimeState = static_cast<uint32_t>(state);
    snapshot.unavailableReason = static_cast<uint32_t>(reason);
    snapshot.lastOpenXrResult = runtime.LastOpenXrResult();
    snapshot.unityInitialized = unityReady.load();
    snapshot.actorBindings = static_cast<uint32_t>(actorBeforeRegistered.load()) +
        static_cast<uint32_t>(actorAfterRegistered.load());
    snapshot.graphicsHooksReady = runtime.GraphicsHooksReady();
    snapshot.backendAttempts = runtime.BackendAttempts();
    snapshot.initializationStatus = initializationStatus.load();
    snapshot.lifecycleName = LifecycleName(snapshot.lifecyclePhase);
    snapshot.runtimeName = gakumas::vr::VrRuntimeStateName(state);
    snapshot.reasonName = gakumas::vr::VrUnavailableReasonName(reason);
    *output = snapshot;
    return GAKU_OK;
}

void Log(GakuLogLevel level, const char* message) noexcept {
    if (logService.write) logService.write(logService.context, level, message);
}

void RuntimeStatus(bool fault, const char* message) noexcept {
    Log(fault ? GAKU_LOG_ERROR : GAKU_LOG_WARNING, message);
}

uint32_t IsVrActive() noexcept {
    if (!activated.load() || stopping.load()) return 0;
    const auto state = gakumas::vr::VrRuntime::Instance().State();
    return state == gakumas::vr::VrRuntimeState::SessionRunning ||
        state == gakumas::vr::VrRuntimeState::PoseReady;
}
uint32_t IsLiveGazeActive() noexcept {
    return IsVrActive() && unityReady.load() && gakumas::vr::liveGazeRequested.load();
}

template<class T> GakuStatus Query(const GakuHostApi* host, const char* name, T& table) {
    T result{};
    auto status = host->queryService(host->hostContext, name,
        GAKU_SERVICE_VERSION, sizeof(result), &result);
    if (status != GAKU_OK) return status;
    if (result.abiVersion != GAKU_SERVICE_VERSION) return GAKU_E_VERSION;
    if (result.structSize < sizeof(result)) return GAKU_E_SIZE;
    table = result;
    return GAKU_OK;
}

GakuStatus PathRoot(const GakuPathsV1& paths, GakuPathKind kind, std::filesystem::path& output) {
    if (!paths.context || !paths.getRoot || !paths.ensureRoot) return GAKU_E_INVALID_ARGUMENT;
    uint32_t count = 0;
    auto status = paths.getRoot(paths.context, kind, nullptr, 0, &count);
    if (status != GAKU_E_SIZE || count == 0 || count > 32768)
        return status == GAKU_E_SIZE ? GAKU_E_SIZE : status == GAKU_OK ? GAKU_E_INTERNAL : status;
    std::vector<wchar_t> buffer(count);
    uint32_t written = 0;
    status = paths.getRoot(paths.context, kind, buffer.data(), count, &written);
    if (status != GAKU_OK) return status;
    if (written != count || buffer.back() != L'\0') return GAKU_E_INVALID_ARGUMENT;
    std::filesystem::path root(buffer.data());
    if (!root.is_absolute()) return GAKU_E_INVALID_ARGUMENT;
    output = std::move(root);
    return GAKU_OK;
}

void Before(void*, void* actor) noexcept {
    if (!stopping.load() && unityReady.load()) GakumasVR::Hosted::ActorBefore(actor);
}
void After(void*, void* actor) noexcept {
    if (!stopping.load() && unityReady.load()) GakumasVR::Hosted::ActorAfter(actor);
}

bool Install(void*, void* target, void* detour, void** original, const char* name) {
    return GakumasVR::Hooks::CreateAndEnable(target, detour, original, name);
}
bool Batch(void*, const gakumas::vr::HookRegistrar::Request* requests, std::size_t count) {
    std::vector<GakumasVR::Hooks::Request> batch;
    batch.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& r = requests[i];
        batch.push_back({r.target, r.detour, r.original, r.diagnosticName});
    }
    return GakumasVR::Hooks::CreateAndEnableBatch(batch.data(), count);
}

void DisableLocalifyBusiness() {
    using namespace GakumasLocal::Config;
    enabled = false;
    lazyInit = false;
    vrLocalizeText = false;
    replaceFont = false;
    replaceTexture = false;
    useMasterTrans = false;
    forceExportResource = false;
    dumpText = false;
    dumpRuntimeTexture = false;
    enableFreeCamera = false;
    enableGyroEmu = false;
    unlockAllLive = false;
    unlockAllLiveCostume = false;
    enableLiveCustomeDress = false;
    loginAsIOS = false;
    useCustomeGraphicSettings = false;
    enableBreastParam = false;
    dmmUnlockSize = false;
    isConfigInit = true;
}

void Shutdown(void*) noexcept {
    if (!activated.load() || stopping.exchange(true)) return;
    GakumasVR::Hosted::CloseUnityAdmission();
    unityReady.store(false);
    gakumas::vr::SetLiveGazeRequested(false);
    if (actorBeforeRegistered)
        actorService.unregisterEntry(actorService.context, GAKU_ACTOR_ENTRY_BEFORE);
    if (actorAfterRegistered)
        actorService.unregisterEntry(actorService.context, GAKU_ACTOR_ENTRY_AFTER);
    actorBeforeRegistered = actorAfterRegistered = false;
    // Stop/join before releasing our leases; never call this under loader lock.
    gakumas::vr::StopVrRuntime();
    GakumasVR::Hooks::Shutdown();
    lifecyclePhase.store(GAKU_VRIFY_PHASE_STOPPED);
    Log(GAKU_LOG_INFO, "GakuVRify stopped; host retains modules and trampolines");
}

void AbortRuntimeInitialization() noexcept {
    lifecyclePhase.store(GAKU_VRIFY_PHASE_FAILED);
    initializationStatus.store(GakumasVR::Hooks::FirstHostFailure() != GAKU_OK ?
        GakumasVR::Hooks::FirstHostFailure() : GAKU_E_UNAVAILABLE);
    GakumasVR::Hosted::CloseUnityAdmission();
    unityReady.store(false);
    gakumas::vr::SetLiveGazeRequested(false);
    if (actorBeforeRegistered)
        actorService.unregisterEntry(actorService.context, GAKU_ACTOR_ENTRY_BEFORE);
    if (actorAfterRegistered)
        actorService.unregisterEntry(actorService.context, GAKU_ACTOR_ENTRY_AFTER);
    actorBeforeRegistered = actorAfterRegistered = false;
    gakumas::vr::StopVrRuntime();
    GakumasVR::Hooks::Shutdown();
}

GakuStatus Activate(const GakuHostApi* host, void** state) noexcept {
    if (!host || !state) return GAKU_E_INVALID_ARGUMENT;
    if (host->abiVersion != GAKU_ABI_VERSION) return GAKU_E_VERSION;
    if (host->structSize < sizeof(*host)) return GAKU_E_SIZE;
    if (!host->queryService || !host->registerService) return GAKU_E_INVALID_ARGUMENT;
    if (activated.load()) return GAKU_E_CONFLICT;
    try {
        GakuHookService hooks{};
        GakuStatus status;
        if ((status = Query(host, GAKU_SERVICE_LOG, logService)) != GAKU_OK) return status;
        if ((status = Query(host, GAKU_SERVICE_RUNTIME, runtimeService)) != GAKU_OK) return status;
        if ((status = Query(host, GAKU_SERVICE_HOOKS, hooks)) != GAKU_OK) return status;
        if ((status = Query(host, GAKU_SERVICE_ACTOR_ENTRY, actorService)) != GAKU_OK) return status;
        if (!logService.write || !runtimeService.gameRoot || !runtimeService.localRoot ||
            !runtimeService.getGameAssembly || !hooks.acquireBatch || !hooks.release ||
            !actorService.registerEntry || !actorService.unregisterEntry) return GAKU_E_UNAVAILABLE;
        GakumasVR::Hosted::ConfigureSharedCameraHost(host);

        const std::filesystem::path gameRoot(runtimeService.gameRoot);
        if (!gameRoot.is_absolute()) return GAKU_E_INVALID_ARGUMENT;
        gakumasLocalPath = runtimeService.localRoot;
        ProgramConfigJson = gakumasLocalPath / L"config.json";
        ConfigJson = gakumasLocalPath / L"localizationConfig.json";
        VrConfigJson = gameRoot / L"gakumas-vr" / L"config.json";
        // Read only known VR fields from the same installation's mixed legacy
        // config, without invoking Localify business or copying its resources.
        GakumasLocal::Config::LoadConfig("{}");
        DisableLocalifyBusiness();
        GakuPathsV1 paths{};
        std::filesystem::path configRoot, logRoot, pluginDataRoot;
        const auto pathStatus = Query(host, GAKU_SERVICE_PATHS, paths);
        bool rootsReady = pathStatus == GAKU_E_UNAVAILABLE;
        if (pathStatus == GAKU_OK)
            rootsReady = PathRoot(paths, GAKU_PATH_CONFIGS, configRoot) == GAKU_OK &&
                PathRoot(paths, GAKU_PATH_LOGS, logRoot) == GAKU_OK &&
                PathRoot(paths, GAKU_PATH_PLUGIN_DATA, pluginDataRoot) == GAKU_OK;
        bool managedLayout = false;
        if (rootsReady) {
            GakumasLocal::Config::InitializeHostedVrConfig(gameRoot / L"gakumas-vr", ConfigJson,
                configRoot, VrConfigJson, managedLayout, pluginDataRoot, logRoot);
            vrLogDirectory = managedLayout ? logRoot : std::filesystem::path{};
        } else {
            Log(GAKU_LOG_ERROR, "VR paths service failed; configuration unchanged and VR disabled");
            // A host may retry an activation that failed later in registration.
            // Do not let previous VR globals become this attempt's startup decision.
            return GAKU_E_UNAVAILABLE;
        }
        DisableLocalifyBusiness();
        GakumasLocal::Config::vrRuntimeStartupEnabled = GakumasLocal::Config::vrEnabled;
        GakumasLocal::Config::vrDiagnosticsStartupEnabled =
            GakumasLocal::Config::vrEnabled && GakumasLocal::Config::vrDiagnosticsEnabled;

        if (!GakumasVR::Hooks::BindHost(hooks, logService)) return GAKU_E_UNAVAILABLE;
        const GakuVrifyService service{GAKU_VRIFY_SERVICE_VERSION, sizeof(GakuVrifyService),
            IsVrActive, IsLiveGazeActive};
        status = host->registerService(host->hostContext, GAKU_VRIFY_SERVICE, sizeof(service), &service);
        if (status != GAKU_OK) { GakumasVR::Hooks::Shutdown(); return status; }
        const auto cameraBegin = GakumasVR::Hosted::cameraBeginProvider.Service();
        status = host->registerService(host->hostContext, GAKU_VRIFY_CAMERA_BEGIN_SERVICE,
            sizeof(cameraBegin), &cameraBegin);
        if (status != GAKU_OK) { GakumasVR::Hooks::Shutdown(); return status; }
        const GakuVrifyDiagnosticsService diagnostics{GAKU_VRIFY_DIAGNOSTICS_VERSION,
            sizeof(GakuVrifyDiagnosticsService), GetSnapshot};
        status = host->registerService(host->hostContext, GAKU_VRIFY_DIAGNOSTICS_SERVICE,
            sizeof(diagnostics), &diagnostics);
        if (status != GAKU_OK) { GakumasVR::Hooks::Shutdown(); return status; }
        activated.store(true);
        lifecyclePhase.store(GAKU_VRIFY_PHASE_ACTIVATED);
        *state = &activated;
        Log(GAKU_LOG_INFO, GakumasLocal::Config::vrEnabled ?
            "GakuVRify activated; waiting for runtime-ready (Localify business disabled)" :
            "GakuVRify activated with VR disabled; check the selected VR config or initialization error and restart");
        return GAKU_OK;
    } catch (const std::exception& e) {
        Log(GAKU_LOG_ERROR, e.what());
    } catch (...) { Log(GAKU_LOG_ERROR, "GakuVRify activation failed"); }
    GakumasVR::Hooks::Shutdown();
    return GAKU_E_INTERNAL;
}

void RuntimeReady(void*) noexcept {
    if (!activated.load() || stopping.load() || runtimeNotified.exchange(true) ||
        !GakumasLocal::Config::vrRuntimeStartupEnabled) return;
    try {
        lifecyclePhase.store(GAKU_VRIFY_PHASE_RUNTIME_READY);
        if (!GakumasLocal::Config::vrNativeOnly) {
            if (!runtimeService.getGameAssembly()) {
                Log(GAKU_LOG_ERROR, "GakuVRify runtime-ready without GameAssembly; VR not started");
                AbortRuntimeInitialization();
                return;
            }
            if (!GakumasVR::Hosted::InitializeUnity() ||
                GakumasVR::Hooks::FirstHostFailure() != GAKU_OK) {
                Log(GAKU_LOG_ERROR, "GakuVRify critical Unity hooks unavailable; VR not started");
                AbortRuntimeInitialization();
                return;
            }
            const GakuActorEntryBinding before{GAKU_SERVICE_VERSION, sizeof(GakuActorEntryBinding),
                GAKU_ACTOR_ENTRY_BEFORE, "VRify actor sampling", Before, nullptr};
            const GakuActorEntryBinding after{GAKU_SERVICE_VERSION, sizeof(GakuActorEntryBinding),
                GAKU_ACTOR_ENTRY_AFTER, "VRify natural gaze", After, nullptr};
            actorBeforeRegistered = actorService.registerEntry(actorService.context, &before) == GAKU_OK;
            actorAfterRegistered = actorService.registerEntry(actorService.context, &after) == GAKU_OK;
            if (!actorBeforeRegistered || !actorAfterRegistered) {
                Log(GAKU_LOG_ERROR, "GakuVRify actor participants unavailable; VR not started");
                AbortRuntimeInitialization();
                return;
            }
            unityReady.store(true);
            lifecyclePhase.store(GAKU_VRIFY_PHASE_UNITY_READY);
        }
        gakumas::vr::VrRuntimeConfig config;
        config.enabled = true;
        config.diagnosticsEnabled = GakumasLocal::Config::vrDiagnosticsStartupEnabled;
        config.cameraPoseBridgeEnabled = GakumasLocal::Config::vrHeadPoseEnabled &&
            !GakumasLocal::Config::vrNativeOnly;
        config.stereoProjectionEnabled = GakumasLocal::Config::vrStereoEnabled && config.cameraPoseBridgeEnabled;
        config.stereoLandscapeOnly = GakumasLocal::Config::vrStereoLandscapeOnly;
        config.stereoRenderScale = GakumasLocal::Config::vrStereoRenderScale;
        config.applicationDirectory = runtimeService.gameRoot;
        config.logDirectory = vrLogDirectory;
        config.statusSink = RuntimeStatus;
        gakumas::vr::HookRegistrar registrar{nullptr, Install, Batch};
        if (!gakumas::vr::StartVrRuntime(std::move(config), registrar)) {
            AbortRuntimeInitialization();
            Log(GAKU_LOG_ERROR, "GakuVRify XR worker failed to start");
        } else {
            lifecyclePhase.store(GAKU_VRIFY_PHASE_WORKER_STARTED);
            Log(GAKU_LOG_INFO, "GakuVRify XR worker started; session/render acceptance pending");
        }
    } catch (const std::exception& e) {
        Log(GAKU_LOG_ERROR, e.what());
        AbortRuntimeInitialization();
    } catch (...) {
        Log(GAKU_LOG_ERROR, "GakuVRify runtime initialization failed");
        AbortRuntimeInitialization();
    }
}
}

void GakuVrifyHostLog(int priority, const char* message) noexcept {
    Log(priority >= 3 ? GAKU_LOG_ERROR : GAKU_LOG_INFO, message ? message : "");
}

extern "C" GAKU_PLUGIN_EXPORT GakuStatus GakuPlugin_Query(
    uint32_t abi, uint32_t size, GakuPluginDescriptor* output) {
    if (abi != GAKU_ABI_VERSION) return GAKU_E_VERSION;
    if (!output) return GAKU_E_INVALID_ARGUMENT;
    if (size < sizeof(*output)) return GAKU_E_SIZE;
    GakuPluginDescriptor descriptor{};
    descriptor.abiVersion = GAKU_ABI_VERSION;
    descriptor.structSize = sizeof(descriptor);
    descriptor.id = "gaku.vrify";
    descriptor.name = "GakuVRify";
    descriptor.version = GAKUMAS_VR_VERSION;
    descriptor.activate = Activate;
    descriptor.onRuntimeReady = RuntimeReady;
    descriptor.shutdown = Shutdown;
    *output = descriptor;
    return GAKU_OK;
}
