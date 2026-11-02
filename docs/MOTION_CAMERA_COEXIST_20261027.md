# Motion camera coexistence — 2026-10-27

**Known Issue:** manual Motion camera movement/rotation was accepted without VR; Motion camera control becomes inactive with VR active. That remaining scheduling work was deferred. The earlier headset picture and one-motion playback acceptance did not cover Motion camera control.

This dated report describes a historical paired build. Source-level checks, deployment hashes and manual acceptance are recorded separately and do not certify a newly compiled DLL.

## Failure and shared boundary

VRify owned both `RenderPipeline.BeginCameraRendering` and `RenderPipelineManager.BeginCameraRendering` before Motion. Their exclusive leases also existed without an available headset/runtime. Motion's competing lease failed, disabling its camera bridge; connecting a device could not resolve ownership.

VRify now publishes `gaku.vrify.camera-begin` v1 for those boundaries. Motion validates both target addresses, subscribes, and skips only the conflicting leases. VR work runs before the real game original, the original runs exactly once, and Motion's existing BeforeRender follows it. VR eye cameras are excluded. Existing Localify Transform-write observation and Motion lens/pose/restoration paths are preserved.

A missing or empty provider retains the independent path. Partial installation, bad ABI and target mismatch fail visibly. Aliased begin addresses use one detour. No original/trampoline pointer is shared, and no generic hook chain or Loader core change is introduced. Provider snapshots run outside the mutex; closing/unsubscribing stops new admission while captured resident callbacks may complete. Hot unload is unsupported.

Localify camera-render arbitration also requires a running XR session and ready Unity. Missing runtime/device, dormant, stopped and faulted states do not suppress Localify's original camera behavior.

## Historical offline verification

- The Motion implementation comparison exercised the actual handshake/callback, two VRify begin-hook bodies and shared-camera activity logic against the provider. Coverage included call order, no-XR notification, eye exclusion, thread checks, absent/empty/malformed/mismatching providers, aliases, exceptions, reentry/unsubscribe and Close.
- Production DLL lifecycle/ABI passed 9/9; hook facade and source-layout checks passed.
- VR-less integration passed 13/13 with no timeout. These cross-component tests use separately supplied implementations and are explicit optional maintainer checks, rather than public-build prerequisites.
- Motion reported 1101 assertions with zero failures; ImGui syntax checking passed.
- Historical VRify Release and Motion Release/DiagnosticCapture builds passed. An initial VRify build was rejected because editing overlapped compilation; the final clean build/package passed with the recorded inputs.

Fixtures do not prove actual camera movement, VR head-pose composition, scene transitions or screenshot safety. Public provider/mock-host checks and source-level implementation comparisons have different dependency/evidence boundaries; see [BUILDING.md](../BUILDING.md).

## Historical paired artifacts and recovery

The recorded Motion baseline was `b227adf`, with diagnostic DLL SHA256 `0f8fc1b692aa8573eb76188726ac56499bfc9d2fcf97425d657542fffbbe7ed2`; it was not the older binary used for the previous playback test.

| Candidate | SHA256 |
| --- | --- |
| VRify Release | `39710ae3f96f0cf8a890a1716df28d42ebc61c12f072b8f29bebc160ab27bd72` |
| Motion DiagnosticCapture | `c46869e0f4484934551cc40d030dc911fadccbc8ab33cb35c1c05e49399bcb27` |
| Motion Release | `69a5e9d6a1600799328f1305cdc6f21a9148cb0c966f2e6aecbfde71c9cee2b6` |

The historical local recovery directory held a manifest, backups and a drift-checking restore procedure. Those machine-local deployment files are not bundled here. After normal game exit, both candidate DLLs matched the table; host, Localify and recorded configuration hashes remained unchanged. Disk verification alone was not visual acceptance.

## Manual acceptance and remaining work

The user manually confirmed Motion camera movement and rotation without a VR device. With VR connected and active, Motion camera control became inactive because its scheduling was not yet aligned with VRify's official-camera-scheduler boundary.

That report confirms the stated no-VR manual case only, not imported camera tracks, every scene or simultaneous Motion/VR camera composition. This acceptance update did not claim a fresh-boot log review. The user closed that modification round with the VR-mode behavior recorded as a Known Issue; no additional scheduling implementation is included in this integration.
