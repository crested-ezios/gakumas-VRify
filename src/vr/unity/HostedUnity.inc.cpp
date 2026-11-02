// Included once at the end of the patched upstream Hook translation unit.
#ifdef GAKU_VRIFY_HOSTED
#include "host/GakuVrifyUnity.hpp"
#include "hooks/HookManager.hpp"

namespace GakumasVR::Hosted {
namespace {
std::atomic<bool> unityReady{false};
std::atomic<bool> unityAttempted{false};
std::atomic<bool> unityAccepting{true};
GakuHostApi cameraHost{};
GakuLocalifyCameraRenderService sharedCamera{};
bool sharedCameraSelected = false;
bool sharedCameraValid = false;
bool sharedCameraTargets[GAKULOCALIFY_CAMERA_RENDER_COUNT]{};
std::atomic<bool> sharedCameraConnected{false};
unsigned char sharedCameraUser{};

void QuerySharedCamera() {
    if (!cameraHost.queryService) return;
    GakuLocalifyCameraRenderService candidate{};
    const auto status = cameraHost.queryService(cameraHost.hostContext,
        GAKULOCALIFY_SERVICE_CAMERA_RENDER, GAKULOCALIFY_CAMERA_RENDER_VERSION,
        sizeof(candidate), &candidate);
    if (status == GAKU_E_UNAVAILABLE) return;
    sharedCameraSelected = true;
    if (status != GAKU_OK || candidate.abiVersion != GAKULOCALIFY_CAMERA_RENDER_VERSION ||
        candidate.structSize < sizeof(candidate) || !candidate.context ||
        !candidate.getTarget || !candidate.subscribe || !candidate.unsubscribe) {
        GakumasLocal::Log::ErrorFmt("VRIFY_SHARED_CAMERA incompatible service status=%d", status);
        return;
    }
    sharedCamera = candidate;
    sharedCameraValid = true;
}

class HostedHookInstaller final : public GakumasLocal::HookInstaller {
public:
    HostedHookInstaller() { m_il2cppLibraryPath = "GameAssembly.dll"; }

    void* InstallHook(void* target, void* detour, void** original) override {
        const bool installed = GakumasVR::Hooks::CreateAndEnable(
            target, detour, original, "vrify.unity");
        return installed ? nullptr : reinterpret_cast<void*>(std::intptr_t{-1});
    }

    GakumasLocal::OpaqueFunctionPointer LookupSymbol(const char* name) override {
        const auto module = GetModuleHandleW(L"GameAssembly.dll");
        return module && name ? reinterpret_cast<GakumasLocal::OpaqueFunctionPointer>(
            GetProcAddress(module, name)) : nullptr;
    }
};
} // namespace

void ConfigureSharedCameraHost(const GakuHostApi* host) noexcept {
    if (host) cameraHost = *host;
}

bool UseSharedCameraTarget(uint32_t kind, void* resolvedTarget) {
    if (!sharedCameraSelected) return false;
    if (kind >= GAKULOCALIFY_CAMERA_RENDER_COUNT) return true;
    const auto installed = sharedCameraValid ?
        sharedCamera.getTarget(sharedCamera.context, kind) : nullptr;
    sharedCameraTargets[kind] = resolvedTarget && installed == resolvedTarget;
    GakumasLocal::Log::InfoFmt("VRIFY_SHARED_CAMERA target kind=%u ready=%d provider=%p expected=%p",
        kind, sharedCameraTargets[kind], installed, resolvedTarget);
    // Selected but unavailable/mismatching providers must fail initialization,
    // never retry the same target through an exclusive hook lease.
    return true;
}

bool ConnectSharedCameraRender() {
    if (!sharedCameraSelected) return true;
    if (!sharedCameraValid) return false;
    for (const auto ready : sharedCameraTargets) if (!ready) return false;
    const GakuLocalifyCameraRenderCallbacks callbacks{
        GAKULOCALIFY_CAMERA_RENDER_VERSION, sizeof(GakuLocalifyCameraRenderCallbacks), &sharedCameraUser,
        [](void*) -> uint32_t {
            return sharedCameraConnected.load(std::memory_order_acquire) &&
                unityReady.load(std::memory_order_acquire) && IsUnityAccepting() &&
                (gakumas::vr::VrRuntime::Instance().State() == gakumas::vr::VrRuntimeState::SessionRunning ||
                 gakumas::vr::VrRuntime::Instance().State() == gakumas::vr::VrRuntimeState::PoseReady);
        },
        [](void*, void** output) {
            if (!output) return;
            auto* camera = static_cast<UnityResolve::UnityType::Camera*>(*output);
            GakumasLocal::HookMain::ObserveMainCamera(camera);
            *output = camera;
        },
        [](void*, void* self, void* state) { GakumasLocal::HookMain::BeforeCameraState(self, state); },
        [](void*, void* self) { GakumasLocal::HookMain::AfterCameraState(self); },
        [](void*, void* camera) { (void)GakumasLocal::HookMain::AfterEndCamera(camera); }};
    const auto status = sharedCamera.subscribe(sharedCamera.context, &callbacks);
    sharedCameraConnected.store(status == GAKU_OK, std::memory_order_release);
    GakumasLocal::Log::InfoFmt("VRIFY_SHARED_CAMERA subscribe status=%d", status);
    return status == GAKU_OK;
}

bool SharedCameraTargetReady(uint32_t kind) noexcept {
    return kind < GAKULOCALIFY_CAMERA_RENDER_COUNT &&
        sharedCameraConnected.load(std::memory_order_acquire) && sharedCameraTargets[kind];
}

bool IsUnityAccepting() noexcept {
    return unityAccepting.load(std::memory_order_acquire);
}

void CloseUnityAdmission() noexcept {
    cameraBeginProvider.Close();
    unityAccepting.store(false, std::memory_order_release);
    unityReady.store(false, std::memory_order_release);
    if (sharedCameraConnected.exchange(false, std::memory_order_acq_rel)) {
        try {
            const auto status = sharedCamera.unsubscribe(sharedCamera.context, &sharedCameraUser);
            if (status != GAKU_OK) GakumasLocal::Log::ErrorFmt(
                "VRIFY_SHARED_CAMERA unsubscribe status=%d", status);
        } catch (...) { OutputDebugStringA("VRify shared camera unsubscribe failed"); }
    }
}

bool InitializeUnity() {
    using namespace GakumasLocal;
    if (!Config::vrRuntimeStartupEnabled || Config::vrNativeOnly) return true;
    if (unityAttempted.exchange(true, std::memory_order_acq_rel)) {
        return unityReady.load(std::memory_order_acquire);
    }
    // Hosted builds never load the old Localify business layer. The supported
    // configuration remains VR-only even if an old JSON enabled that layer.
    Config::enabled = false;
    Config::vrLocalizeText = false;
    if (!Config::isConfigInit || !GetModuleHandleW(L"GameAssembly.dll")) {
        Log::Error("VRIFY_HOSTED_UNITY unavailable=config-or-assembly");
        return false;
    }
    QuerySharedCamera();
    Plugin::GetInstance().InstallHook(std::make_unique<HostedHookInstaller>());
    const bool mainReady = HookMain::Camera_get_main_Orig ||
        SharedCameraTargetReady(GAKULOCALIFY_CAMERA_RENDER_MAIN);
    const bool pushReady = HookMain::CinemachineBrain_PushStateToUnityCamera_Orig ||
        SharedCameraTargetReady(GAKULOCALIFY_CAMERA_RENDER_PUSH);
    const bool endReady = HookMain::EndCameraRendering_Orig ||
        SharedCameraTargetReady(GAKULOCALIFY_CAMERA_RENDER_END);
    const bool ready = mainReady && pushReady && endReady && HookMain::DoRenderLoopInternal_Orig;
    unityReady.store(ready, std::memory_order_release);
    Log::InfoFmt("VRIFY_HOSTED_UNITY ready=%d cameraMain=%d cinemachine=%d endCamera=%d renderLoop=%d",
        ready, mainReady, pushReady, endReady,
        HookMain::DoRenderLoopInternal_Orig != nullptr);
    return ready;
}

void ActorBefore(void* actor) noexcept {
    if (!actor || !IsUnityAccepting() || !unityReady.load(std::memory_order_acquire)) return;
    try {
        gakumas::vr::TraceLiveGazeStage(actor, nullptr, "actor-late-before");
        GakumasLocal::HookMain::SampleVrActor(actor);
    } catch (...) {
        GakumasLocal::Log::Error("VRIFY_HOSTED_ACTOR failed=before");
    }
}

void ActorAfter(void* actor) noexcept {
    if (!actor || !IsUnityAccepting() || !unityReady.load(std::memory_order_acquire)) return;
    try {
        gakumas::vr::TickNaturalLiveGaze(actor);
        gakumas::vr::TraceLiveGazeStage(actor, nullptr, "actor-late-after");
    } catch (...) {
        GakumasLocal::Log::Error("VRIFY_HOSTED_ACTOR failed=after");
    }
}
} // namespace GakumasVR::Hosted
#endif
