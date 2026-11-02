# GakuVRify plugin integration

This branch builds the current GakuLoader-hosted `GakuVRify.dll`. It includes Localify camera/render sharing and the Motion begin-camera provider. The dated reports below describe particular artifacts and observations; they do not automatically validate a newly built DLL.

## Build and package

Follow [BUILDING.md](../BUILDING.md) for toolchain/dependency setup:

```powershell
./build.ps1 -Mode Plugin -Configuration Release
./tests/test-hosted-plugin.ps1 -Configuration Release
./package-plugin.ps1 -Configuration Release
```

The default Loader SDK is [`sdk/gakuloader/`](../sdk/gakuloader/PROVENANCE.md). `-LoaderSdkDirectory` can explicitly select a compatible alternate ABI header. Plugin compilation and public mock-host tests do not require the Loader implementation or private sibling repositories.

Output: `build/bin/plugin/x64/Release/GakuVRify.dll`; package: `build/plugin-package/`.

```text
gakumas-local/plugins/GakuVRify.dll
gakumas-local/tools/gaku.vrify/vr-config-migration.exe
openxr_loader.dll
gakumas-vr/config.example.json
gakumas-vr/version.txt
gakumas-vr/licenses/
plugin-package.json
```

Install GakuLoader separately. With the game closed, `install-plugin.ps1 -GameRoot '<game directory>'` verifies package/build receipts and records program-file backups plus `Restore.ps1` in `build/plugin-deployments/`. It does not replace GakuLoader's `version.dll`, write active configuration or install translation data. Compatibility preflight may read known VR fields from same-install legacy settings. Running games are refused.

Fresh settings use `gakumas-local/Configs/gaku.vrify/config.json` with a current Loader paths service. State and future diagnostics use `PluginData/gaku.vrify` and `Logs/gaku.vrify`. Existing dedicated settings stay in place until the native tool's explicit plan/backup/apply migration; original configs, receipts and logs are retained. The package ships an example only. See [first-use compatibility](CONFIG_LAYOUT_20261102.md) and [standard-layout migration](CONFIG_MIGRATION_V2_20261102.md) for commands, precedence and recovery. `Restore.ps1` restores package files; it does not reverse the data migration.

The generic installer conservatively refuses full Unity VR alongside Localify/Motion. It cannot establish that those external components provide the matching bridge versions, and it does not upgrade the whole provider/consumer pair. The bridges are implemented; historical coordinated deployments are described in [camera/render coexistence](CAMERA_COEXIST_20261027.md) and [Motion camera coexistence](MOTION_CAMERA_COEXIST_20261027.md). Do not treat those records as a generic upgrade script.

Standalone mode remains explicit: `build.ps1 -Mode Standalone`, `package-vr.ps1`, and `install-to-game.ps1 -Mode Standalone`. The upstream standalone Releases are separate from this plugin package.

## Runtime and shared interfaces

- `GakuPlugin_Query` negotiates Loader ABI v1 without starting threads or XR. Activation queries log/runtime/hooks/actor-entry and the optional paths service. It consumes committed standard settings, or compatible legacy settings until explicit migration. Known mixed VR fields can be imported once without Localify resource business. Fresh initialization uses embedded release defaults. Invalid/unsupported settings or missing committed data disable VR without overwriting the original or silently falling back.
- Runtime-ready uses the host's attached IL2CPP worker. Plugin mode excludes the second proxy bootstrap, Localify desktop GUI/resource updater and nested plugin scanner.
- Executable hook requests use the host's owner-bound `acquireBatch`. No MinHook implementation is linked into the plugin. A failed initial lease batch retains originals/status, prevents XR startup and releases only this owner's leases.
- The host owns `CampusActorController::LateUpdate`; VRify registers BEFORE sampling and AFTER natural-gaze callbacks. This does not establish final-pose/gaze composition with Motion's later actor callback.
- Shutdown closes Unity admission, unregisters actor callbacks, stops XR and releases owned leases. Resident contexts/trampolines may outlive closed admission. Hot unload/restart and a full callback-drain guarantee are unsupported.

| Interface | Purpose and boundary |
| --- | --- |
| `gaku.vrify.service` v1 | Read-only active VR/gaze queries; a running session is required, and calls return false after shutdown. It exposes no uninitialized head pose. |
| `gaku.vrify.diagnostics` v1 | Size-checked lifecycle, Unity/actor/graphics readiness, backend attempts, raw XR result and explicit unavailable reason. |
| `gakulocalify.camera-render` v1 | Optional external provider for main-camera, Cinemachine and end-camera targets. VRify validates exact resolved targets before subscribing, without sharing a foreign trampoline. |
| `gaku.vrify.camera-begin` v1 | VRify publishes the two begin-camera boundaries for Motion. The original runs once; VR runs before it and Motion notification after it. VR eye cameras are excluded. |

Absent providers retain the independent path. Incompatible ABI/targets fail visibly rather than stealing another owner's exclusive hook. Camera-render arbitration requires a running XR session and ready Unity; unavailable/stopped states do not suppress Localify's original camera behavior.

Support translation units from the pinned Localify upstream still compile for metadata, camera math, configuration and menu strings; translation/resource business is disabled in hosted mode. This is not complete source/dependency removal. A desktop parameter panel and final-pose/gaze coordination are separate work.

## Validation and known limitations

Public default validation uses source-pipeline fixtures, the public camera provider contract and the production DLL with a mock host. It does not compile private core implementations. See [BUILDING.md](../BUILDING.md) for exact commands.

Production-host/Localify-provider smoke testing requires `-RunMaintainerIntegration` and separately supplied source trees. The Motion implementation comparison is also an explicit maintainer check. The installed-runtime/device availability probe is a separate manual diagnostic source, not invoked by the smoke runner or default public checks. None of these are normal public-build prerequisites.

Historical reports preserve their respective input versions:

- [Initial DLL lifecycle/facade checks](PLUGIN_VALIDATION_20261026.md).
- [Enabled VR-less startup](VRLESS_SMOKE_20261026.md): synthetic metadata/backend controls and the pinned official loader, without successful XR rendering.
- [Localify camera/render coexistence](CAMERA_COEXIST_20261027.md): shared targets plus a paired-build headset picture and one Motion playback.
- [Motion camera coexistence](MOTION_CAMERA_COEXIST_20261027.md): no-VR manual movement/rotation accepted; **Motion camera control with VR active remains a Known Issue**.

These checks do not certify all scenes, devices, imported camera tracks, controller behavior, XR loss, final-pose/gaze ordering or every shutdown path. Keep compilation, package verification, process loading and visual acceptance separate.
