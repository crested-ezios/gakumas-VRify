#include "HostedHooks.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

namespace GakumasVR::Hooks {
namespace {
std::mutex mutex;
GakuHookService service{};
GakuLogService logger{};
std::atomic<bool> bound{false};
std::atomic<GakuStatus> lastStatus{GAKU_E_UNAVAILABLE};
std::atomic<GakuStatus> firstFailure{GAKU_OK};
std::vector<void*> targets;
}

bool BindHost(const GakuHookService& hooks, const GakuLogService& log) {
    std::lock_guard lock(mutex);
    if (bound.load() || hooks.abiVersion != GAKU_SERVICE_VERSION ||
        hooks.structSize < sizeof(hooks) || !hooks.acquireBatch || !hooks.release)
        return false;
    service = hooks;
    logger = log;
    lastStatus.store(GAKU_OK);
    firstFailure.store(GAKU_OK);
    bound.store(true, std::memory_order_release);
    return true;
}

bool Initialize() { return IsInitialized(); }
bool IsInitialized() { return bound.load(std::memory_order_acquire); }
GakuStatus LastHostStatus() noexcept { return lastStatus.load(); }
GakuStatus FirstHostFailure() noexcept { return firstFailure.load(); }

bool CreateAndEnableBatch(const Request* requests, std::size_t count) {
    std::lock_guard lock(mutex);
    if (!bound.load()) { lastStatus.store(GAKU_E_SHUTTING_DOWN); return false; }
    if (!requests || count == 0 || count > 256) {
        lastStatus.store(GAKU_E_INVALID_ARGUMENT); return false;
    }
    std::vector<GakuHookRequest> batch;
    batch.reserve(count);
    targets.reserve(targets.size() + count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& r = requests[i];
        batch.push_back({r.target, r.detour, r.original});
    }
    // Never zero originals, split the batch or roll back trampolines here.
    // The host alone handles validation, publication and partial enable failure.
    const auto result = service.acquireBatch(service.context,
        static_cast<uint32_t>(count), batch.data());
    lastStatus.store(result);
    if (result != GAKU_OK) {
        GakuStatus success = GAKU_OK;
        firstFailure.compare_exchange_strong(success, result);
    }
    // Also retain attempted targets on enable failure: callbacks may have run.
    // release() is owner-bound and cannot disable another plugin's hook.
    for (const auto& r : batch) {
        bool seen = false;
        for (auto* target : targets) if (target == r.target) { seen = true; break; }
        if (!seen && r.target) targets.push_back(r.target);
    }
    if (result != GAKU_OK && logger.write) {
        char message[320];
        std::snprintf(message, sizeof(message),
            "VRify hook lease failed: status=%d count=%zu first=%s target=%p",
            result, count, requests[0].name ? requests[0].name : "unnamed", requests[0].target);
        logger.write(logger.context, GAKU_LOG_ERROR, message);
    }
    return result == GAKU_OK;
}

bool CreateAndEnable(void* target, void* detour, void** original, const char* name) {
    const Request request{target, detour, original, name};
    return CreateAndEnableBatch(&request, 1);
}

void Shutdown() {
    std::lock_guard lock(mutex);
    if (!bound.exchange(false)) return;
    for (auto it = targets.rbegin(); it != targets.rend(); ++it)
        service.release(service.context, *it);
    // Host-owned originals, service tables and callback state stay resident.
}
}
