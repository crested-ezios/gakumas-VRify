// Test-only plugin: the real Localify CameraRenderProvider and real host-owned
// detours, without Localify translation/configuration/bootstrap behavior.
#include "CameraRenderProvider.hpp"
#include <Windows.h>
#include <atomic>
#include <cstdint>

namespace {
GkmsHosted::CameraRenderProvider provider;
GakuLocalifyCameraRenderService realService{};
GakuHookService hooks{};
GakuLogService logService{};
std::atomic<std::uint64_t> counters[9]{};
using MainFn = void* (*)(void*);
using PushFn = void (*)(void*, void*, void*);
MainFn originalMain{};
PushFn originalPush{}, originalEnd{};
void* targets[3]{};
void* mismatchedTarget{};

void* Main(void* method) {
    void* camera = originalMain(method);
    provider.DispatchMain(&camera);
    return camera;
}
void Push(void* self, void* state, void* method) {
    const auto dispatch = provider.DispatchBeforePush(self, state);
    originalPush(self, state, method);
    provider.DispatchAfterPush(dispatch, self);
}
void End(void* context, void* camera, void* method) {
    originalEnd(context, camera, method);
    provider.DispatchEnd(camera);
}

void* PublishedTarget(void* context, uint32_t kind) {
    ++counters[6];
    wchar_t value[2]{};
    if (kind == GAKULOCALIFY_CAMERA_RENDER_MAIN &&
        GetEnvironmentVariableW(L"GAKU_VRLESS_CAMERA_TARGET_MISMATCH", value, 2) && value[0] == L'1') {
        ++counters[7];
        return mismatchedTarget;
    }
    return realService.getTarget(context, kind);
}
GakuStatus Subscribe(void* context, const GakuLocalifyCameraRenderCallbacks* callbacks) {
    ++counters[2];
    const auto status = realService.subscribe(context, callbacks);
    if (status == GAKU_OK) ++counters[3];
    return status;
}
GakuStatus Unsubscribe(void* context, void* userData) {
    ++counters[4];
    return realService.unsubscribe(context, userData);
}
GakuStatus Activate(const GakuHostApi* host, void** state) {
    if (!host || !state || !host->queryService || !host->registerService) return GAKU_E_INVALID_ARGUMENT;
    if (host->abiVersion != GAKU_ABI_VERSION || host->structSize < sizeof(*host)) return GAKU_E_VERSION;
    auto status = host->queryService(host->hostContext, GAKU_SERVICE_HOOKS,
        GAKU_SERVICE_VERSION, sizeof(hooks), &hooks);
    if (status != GAKU_OK) return status;
    status = host->queryService(host->hostContext, GAKU_SERVICE_LOG,
        GAKU_SERVICE_VERSION, sizeof(logService), &logService);
    if (status != GAKU_OK) return status;
    realService = provider.Service();
    auto published = realService;
    published.getTarget = PublishedTarget;
    published.subscribe = Subscribe;
    published.unsubscribe = Unsubscribe;
    status = host->registerService(host->hostContext, GAKULOCALIFY_SERVICE_CAMERA_RENDER,
        sizeof(published), &published);
    if (status == GAKU_OK) *state = &provider;
    return status;
}
void RuntimeReady(void*) {
    const auto module = GetModuleHandleW(L"GameAssembly.dll");
    using TargetFn = void* (*)(uint32_t);
    const auto getTarget = module ? reinterpret_cast<TargetFn>(
        GetProcAddress(module, "GakuVrifyTestMetadataHookTarget")) : nullptr;
    if (!getTarget || !hooks.acquireBatch) { ++counters[8]; return; }
    for (uint32_t i = 0; i < 3; ++i) targets[i] = getTarget(i);
    mismatchedTarget = getTarget(3);
    const GakuHookRequest requests[]{
        {targets[0], reinterpret_cast<void*>(Main), reinterpret_cast<void**>(&originalMain)},
        {targets[1], reinterpret_cast<void*>(Push), reinterpret_cast<void**>(&originalPush)},
        {targets[2], reinterpret_cast<void*>(End), reinterpret_cast<void**>(&originalEnd)}};
    ++counters[0];
    const auto status = hooks.acquireBatch(hooks.context, 3, requests);
    if (status == GAKU_OK) {
        counters[1] = 3;
        for (uint32_t i = 0; i < 3; ++i) provider.SetInstalledTarget(i, targets[i]);
    } else ++counters[8];
    if (logService.write) logService.write(logService.context,
        status == GAKU_OK ? GAKU_LOG_INFO : GAKU_LOG_ERROR,
        status == GAKU_OK ? "fixture Localify owns exactly three camera/render leases" :
            "fixture Localify camera/render lease install failed");
}
void Shutdown(void*) { provider.Close(); ++counters[5]; }
}

extern "C" __declspec(dllexport) std::uint64_t GakuVrifyTestCameraProviderCounter(uint32_t id) {
    return id < 9 ? counters[id].load() : 0;
}
extern "C" GAKU_PLUGIN_EXPORT GakuStatus GakuPlugin_Query(
    uint32_t version, uint32_t size, GakuPluginDescriptor* output) {
    if (version != GAKU_ABI_VERSION) return GAKU_E_VERSION;
    if (!output) return GAKU_E_INVALID_ARGUMENT;
    if (size < sizeof(*output)) return GAKU_E_SIZE;
    GakuPluginDescriptor descriptor{};
    descriptor.abiVersion = GAKU_ABI_VERSION; descriptor.structSize = sizeof(descriptor);
    descriptor.id = "fixture.localify-camera-render";
    descriptor.name = "Localify camera/render provider fixture";
    descriptor.version = "1";
    descriptor.activate = Activate; descriptor.onRuntimeReady = RuntimeReady; descriptor.shutdown = Shutdown;
    *output = descriptor;
    return GAKU_OK;
}
