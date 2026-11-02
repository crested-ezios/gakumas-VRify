# Localify camera/render coexistence — 2026-10-27

This is a summary of a historical matched build and its limited runtime acceptance. It preserves technical conclusions and artifact identities without machine-local paths, device identifiers or raw process logs. It does not validate a newly compiled DLL from this checkout.

## Failure and repair

The earlier VRify build stopped before XR startup because Localify already owned `Camera.get_main`, `CinemachineBrain.PushStateToUnityCamera` and `RenderPipeline.EndCameraRendering`. Host leases returned `GAKU_E_CONFLICT`; a returned runtime-ready callback did not imply successful Unity or XR initialization. The retained Motion DLL was not the owner of those three targets.

Localify's `gakulocalify.camera-render` v1 bridge keeps the single detour for each target. VRify validates all three resolved addresses before subscribing. An absent provider keeps the independent path; an incompatible or mismatching provider fails visibly rather than acquiring a competing lease. No foreign trampoline is copied.

- Main camera: game original, then VR observation/override.
- Cinemachine: VR before callback, game original exactly once, VR after callback. One resident snapshot covers the pair, including reentry/unsubscription during the original.
- End camera: game original, then VR end-camera work.

The corresponding Localify free-camera branches yield to an active VR subscriber. This does not certify every free-camera feature; the historical tested configuration had free camera and gyro emulation disabled. Shutdown closes VR admission before unsubscribing, without claiming callback drain or hot unload. Functions and contexts remain resident through process exit.

The host ABI, retained Motion DLL and existing `gakulocalify.camera-write` service were unchanged. Localify provider source revision was `d8de333c3b899fe9884f5e51d2850dec1a982b92`.

## Historical verification

- Both production Release DLLs built successfully.
- Localify source-level tests exercised the three actual hook bodies for original count/order, inactive behavior, exceptions, rebinding/reentry, Close and in-flight snapshots. Existing camera-write, binary ABI and standalone isolation checks passed.
- VR-less integration passed **13/13** scenarios. Two coexistence cases used the actual Localify provider class inside a fixture DLL with real host leases: three shared targets matched, four targets patched/restored, and actor participants returned from two to zero. Wrong targets failed before backend startup.
- A separate installed OpenXR probe reached system/D3D11 requirements with `XR_SUCCESS` under VirtualDesktopXR. It created no session or frames. Its diagnostic source is `tests/vrless/OpenXrAvailabilityProbe.cpp`; the smoke runner does not invoke it.

These source-level cross-component tests require separately supplied implementations and are optional maintainer integration checks. They are not prerequisites for public mock-host tests or normal plugin compilation; see [BUILDING.md](../BUILDING.md).

## Matched deployment and runtime observations

The game was closed before replacement, and both installed DLL hashes matched:

| File | SHA256 |
| --- | --- |
| GakuLocalify.dll | `62d59aeed6641a2146f924ab20955925cae8163f9b43db3f132ebefc385a6075` |
| GakuVRify.dll | `95a48b8b1d175821a1ea155560be34ee8737ae2daa348340f7c3c65bdf20fef7` |

The host proxy, retained Motion and relevant configurations kept their recorded pre-deployment hashes. The historical local recovery manifest/backups are not distributed as an installation package.

A subsequent normal run matched all three shared targets and reported ready camera/render hooks. VirtualDesktopXR with a Meta Quest 3 reached `SessionRunning` and `PoseReady`. Both eye descriptors were 2496×2592; projection submission continued through generation 10800. The two observed scene holds released as `stereo-ready` after 54 ms and 23 ms. These were transient startup/scene-transition waits.

The user confirmed a normal headset picture and one actual GakuMotion playback with normal compatibility. This covers the historical paired build and retained Motion binary; it does not certify all scenes, assets, imported cameras or gaze features. No loaded DLL was replaced during that accepted run. The public [evidence summary](evidence/camera-coexist-20261027.json) retains the relevant input hashes and bounded observations.

## Current limits

Only a matching provider/consumer pair has the bridge; a new VRify cannot repair an old provider automatically. The generic installer remains conservative because it does not validate/upgrade external Localify/Motion components.

The later begin-camera integration restored manual Motion camera movement/rotation without VR. **Motion camera control with VR active remains a Known Issue**, recorded in [Motion camera coexistence](MOTION_CAMERA_COEXIST_20261027.md). Earlier headset picture/playback acceptance must not be expanded to that unresolved case.
