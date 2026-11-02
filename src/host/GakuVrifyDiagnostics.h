#pragma once
#include "GakuPlugin.h"

#define GAKU_VRIFY_DIAGNOSTICS_SERVICE "gaku.vrify.diagnostics"
#define GAKU_VRIFY_DIAGNOSTICS_VERSION 1u
#define GAKU_VRIFY_PHASE_DORMANT 0u
#define GAKU_VRIFY_PHASE_ACTIVATED 1u
#define GAKU_VRIFY_PHASE_RUNTIME_READY 2u
#define GAKU_VRIFY_PHASE_UNITY_READY 3u
#define GAKU_VRIFY_PHASE_WORKER_STARTED 4u
#define GAKU_VRIFY_PHASE_FAILED 5u
#define GAKU_VRIFY_PHASE_STOPPED 6u

#define GAKU_VRIFY_UNAVAILABLE_NONE 0u
#define GAKU_VRIFY_UNAVAILABLE_LOADER_MISSING 1u
#define GAKU_VRIFY_UNAVAILABLE_RUNTIME_MISSING 2u
#define GAKU_VRIFY_UNAVAILABLE_HEADSET_MISSING 3u
#define GAKU_VRIFY_UNAVAILABLE_D3D11_EXTENSION 4u

#ifdef __cplusplus
extern "C" {
#endif
/* Read-only, thread-safe observations. Names are provider-owned process-lifetime
   literals; numeric runtimeState is diagnostic only, use runtimeName. This is
   not a test backend and does not change startup policy. */
typedef struct GakuVrifySnapshot {
    uint32_t abiVersion; uint32_t structSize;
    uint32_t lifecyclePhase;
    uint32_t runtimeState;
    uint32_t unavailableReason;
    int32_t lastOpenXrResult;
    uint32_t unityInitialized;
    uint32_t actorBindings;
    uint32_t graphicsHooksReady;
    uint32_t backendAttempts;
    int32_t initializationStatus;
    const char* lifecycleName;
    const char* runtimeName;
    const char* reasonName;
} GakuVrifySnapshot;

typedef struct GakuVrifyDiagnosticsService {
    uint32_t abiVersion; uint32_t structSize;
    /* Failure leaves output untouched. No Unity/managed or backend calls. */
    GakuStatus (*getSnapshot)(uint32_t outputSize, GakuVrifySnapshot* output);
} GakuVrifyDiagnosticsService;
#ifdef __cplusplus
}
#endif
