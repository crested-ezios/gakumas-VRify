#pragma once
#include "GakuVrifyCameraBegin.h"
#include <mutex>
namespace GakumasVR::Hosted {
class CameraBeginProvider {
    std::mutex mutex_;
    void* targets_[2]{};
    GakuVrifyAfterCameraBegin callback_{};
    void* user_{};
    bool closed_{};
public:
    void SetTarget(uint32_t kind, void* target) {
        std::lock_guard lock(mutex_);
        if (!closed_ && kind < 2) targets_[kind] = target;
    }
    void* Target(uint32_t kind) {
        std::lock_guard lock(mutex_);
        return !closed_ && kind < 2 ? targets_[kind] : nullptr;
    }
    GakuStatus Subscribe(GakuVrifyAfterCameraBegin callback, void* user) {
        if (!callback) return GAKU_E_INVALID_ARGUMENT;
        std::lock_guard lock(mutex_);
        if (closed_) return GAKU_E_SHUTTING_DOWN;
        if (callback_ && (callback_ != callback || user_ != user)) return GAKU_E_CONFLICT;
        callback_ = callback; user_ = user;
        return GAKU_OK;
    }
    GakuStatus Unsubscribe(GakuVrifyAfterCameraBegin callback, void* user) {
        std::lock_guard lock(mutex_);
        if (callback_ && (callback_ != callback || user_ != user)) return GAKU_E_CONFLICT;
        callback_ = nullptr; user_ = nullptr;
        return GAKU_OK;
    }
    void Dispatch(void* camera, bool eye) noexcept {
        if (!camera || eye) return;
        GakuVrifyAfterCameraBegin callback{}; void* user{};
        { std::lock_guard lock(mutex_); if (!closed_) { callback = callback_; user = user_; } }
        try { if (callback) callback(user, camera); } catch (...) {}
    }
    void Close() { std::lock_guard lock(mutex_); closed_ = true; callback_ = nullptr; user_ = nullptr; }
    GakuVrifyCameraBeginService Service() {
        return {1, sizeof(GakuVrifyCameraBeginService), this,
            [](void* p, uint32_t k) -> void* { return static_cast<CameraBeginProvider*>(p)->Target(k); },
            [](void* p, GakuVrifyAfterCameraBegin cb, void* u) -> GakuStatus { return static_cast<CameraBeginProvider*>(p)->Subscribe(cb,u); },
            [](void* p, GakuVrifyAfterCameraBegin cb, void* u) -> GakuStatus { return static_cast<CameraBeginProvider*>(p)->Unsubscribe(cb,u); }};
    }
};
inline CameraBeginProvider cameraBeginProvider;
}
