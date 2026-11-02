#pragma once
#include "GakuPlugin.h"

#define GAKU_VRIFY_SERVICE "gaku.vrify.service"
#define GAKU_VRIFY_SERVICE_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif
/* Provider-owned, thread-safe status queries. VR active means an XR session is
   running; live gaze means VRify currently requests gaze ownership. No Unity
   object access or private C++/OpenXR layouts cross this boundary. */
typedef struct GakuVrifyService {
    uint32_t abiVersion;
    uint32_t structSize;
    uint32_t (*isVrActive)(void);
    uint32_t (*isLiveGazeActive)(void);
} GakuVrifyService;
#ifdef __cplusplus
}
#endif
