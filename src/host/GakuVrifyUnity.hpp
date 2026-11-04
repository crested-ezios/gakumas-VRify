#pragma once
#ifdef GAKU_VRIFY_HOSTED
#include "GakuLocalifyCameraRender.h"
#include "CameraBeginProvider.hpp"
#endif

namespace GakumasVR::Hosted {
// Runtime-ready worker: metadata resolution and lease installation only.
// False means that at least one required camera/render entry is unavailable.
// Hook leases and any published originals remain process-lifetime on failure.
bool InitializeUnity();
#ifdef GAKU_VRIFY_HOSTED
// Optional, provider-owned bridge for exactly three known game entries. An
// absent provider or null target keeps VRify's independent hook path; only an
// exact non-null provider target waives that target's exclusive hook lease.
// An incompatible provider or non-null mismatch never falls through to a
// competing detour acquisition.
void ConfigureSharedCameraHost(const GakuHostApi* host) noexcept;
bool UseSharedCameraTarget(uint32_t kind, void* resolvedTarget);
bool ConnectSharedCameraRender();
bool SharedCameraTargetReady(uint32_t kind) noexcept;
#endif
// Closing this process-lifetime admission prevents new Unity-side VR work.
// It does not wait for callbacks that already passed their admission check.
bool IsUnityAccepting() noexcept;
void CloseUnityAdmission() noexcept;

// GakuLoader actor-entry participants. BEFORE samples anchors before the game
// original; AFTER applies gaze before the loader's onActorTick dispatch.
void ActorBefore(void* actor) noexcept;
void ActorAfter(void* actor) noexcept;
} // namespace GakumasVR::Hosted
