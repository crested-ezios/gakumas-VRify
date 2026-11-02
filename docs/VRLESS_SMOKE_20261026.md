# VR-less smoke test - 2026-10-26

The production GakuVRify DLL now passes a smoke test through its enabled startup
path without an external Localify module or a headset. This test checks the
initialization boundary and actual host hook leases; it does not certify real
Unity scenes or VR rendering.

## Design

The runner builds a native executable from the sibling GakuLoader's production
`PluginHost`, `ActorEntry`, `Hotkeys`, trace code and vendored MinHook. No host
test-seam macro is defined. Only the desktop UI/dialog shell is stubbed. The host
loads the production `GakuVRify.dll`, publishes real owner-bound services, calls
activation/runtime-ready/shutdown, and installs and restores actual detours.

Every scenario runs in a new child process and an isolated directory. No Localify
DLL, translation/configuration data, Vuplex module or legacy proxy is present.
Activation occurs before any GameAssembly is loaded. Module inventories, host and
VR logs, lifecycle snapshots and backend/metadata counters are recorded. A child
process deadline detects crashes or hangs without touching an installed game.

There are three positive layers and two negative controls:

| Layer | Real execution and expected boundary |
|---|---|
| Native-only | No GameAssembly at all. Real host activation and D3D11 leases, then absent loader / absent runtime / absent HMD. |
| Hosted Unity startup | A minimal GameAssembly metadata fixture is loaded after activation. The production initializer enumerates it, installs four critical camera/render detours and two actor participants, then reaches the same three VR boundaries. |
| Official Khronos loader | Both native and Unity paths load the pinned official 1.1.61 loader. A child-only `XR_RUNTIME_JSON` points to an intentionally absent file, producing actual `XR_ERROR_RUNTIME_UNAVAILABLE`. |
| Invalid loader control | A corrupt app-local OpenXR DLL must report `Faulted`, with no unavailable reason. It must not pass as missing VR hardware. |
| Missing metadata control | Hiding the fixture's critical Camera.get_main metadata must report `initialization-failed`, with zero backend attempts and no unavailable reason. Already installed leases must be released. |

The thin OpenXR test backend supplies only extension/instance metadata dispatch.
One mode returns `XR_ERROR_RUNTIME_UNAVAILABLE` from xrCreateInstance; the other
returns `XR_ERROR_FORM_FACTOR_UNAVAILABLE` from xrGetSystem. It never supplies a
system, session, graphics binding, controller action, pose or frame. Unexpected
session/pose calls fail and are counted. The real-loader scenarios ensure the
runtime-unavailable conclusion also holds outside this fake backend.

## Observable production behavior

- Native-only initialization no longer requires a GameAssembly handle. Full
  Unity mode still requires valid game metadata, independently of Localify.
- `gaku.vrify.diagnostics` v1 exposes a read-only, size-checked lifecycle snapshot.
  It reports Unity/actor readiness, graphics-hook readiness, backend attempts,
  raw XR result, and an explicit unavailable reason. The existing
  `gaku.vrify.service` v1 table is unchanged.
- Missing loader/runtime/HMD is `Unavailable`, not successful VR activation or
  a Localify initialization error. Invalid backend/Unity initialization remains
  a separate fault. No fake backend is compiled into the production plugin.
- Unavailable/fault transitions write to the native host log even with detailed
  VR file logging disabled and no console. The native-loader-missing scenario
  explicitly tests this configuration.
- Runtime/HMD absence retains the existing retry policy. The smoke runner waits
  for the explicit unavailable snapshot and host-log message, then requests normal
  host shutdown. It verifies cancellation, participant removal and restoration
  of the four original hook targets. Retry is not treated as a hang.

## Reproduce

Execution result: **10/10 passed**, all child processes exited with code 0 and no
deadline expired. The slowest host shutdown was **563 ms**, against a 4,000 ms
assertion. All four positive Unity scenarios installed/restored four critical
targets and removed both actor participants. Managed/object invocation counters
and fake session/pose call counters stayed zero. Every unavailable scenario wrote
its reason to the host log, including the case with VR detailed logging disabled.

The tested production DLL SHA256 is
`f4b8d9b77984a74a780af335575a7d469580cf5698054680d7da006985ff1e14`.

| Expected stop | Result |
|---|---|
| Missing app-local loader | `Unavailable / loader-missing`; no XR API result fabricated |
| Missing runtime, fake and official loaders | `Unavailable / runtime-missing`, XR result `-51` |
| Missing HMD after instance creation | `Unavailable / headset-missing`, XR result `-35` |
| Invalid loader | `Faulted`, unavailable reason `none` |
| Missing critical game metadata | `initialization-failed`, zero backend attempts, unavailable reason `none` |

Optional maintainer reproduction requires separately supplied GakuLoader host/MinHook and Localify provider implementation sources. These are not bundled or needed for the public build/mock-host checks:

```powershell
./build.ps1 -Mode Plugin -Configuration Release
./package-plugin.ps1
./tests/test-vrless-smoke.ps1 -RunMaintainerIntegration -GakuLoaderRoot '<host source>' -LocalifyRoot '<provider source>'
```

The package command obtains/verifies the pinned official loader. The smoke runner
also accepts `-PluginPath`, `-GakuLoaderRoot`, and `-RealOpenXrLoader`; it verifies
the official loader hash. It never installs the test DLLs into a game directory.
Results are under `build/vrless-smoke/run-*/report.json`, with separate per-scenario
JSON, stdout/stderr, host logs and (when enabled) VR logs. Source and artifact hashes
are included. The checked-in execution summary is
[`evidence/vrless-smoke-20261026.json`](evidence/vrless-smoke-20261026.json).

## Evidence boundary

The metadata fixture contains only game/Unity/Cinemachine image and method shapes;
it exports no Localify bootstrap or Localify service symbols. Tests check that
metadata enumeration occurred, that startup invoked no managed/object methods,
and that four critical entry points were really patched/restored by the host.
Optional game metadata deliberately absent from the fixture is not certified.

Thus the covered enabled startup path has no external Localify initialization or
bootstrap prerequisite. Pinned public upstream Localify-derived support code
still exists in the plugin. Actual GakuLoader window/Vuplex startup, real game
metadata compatibility, scenes, a successful XR session, rendering/input and
multi-plugin coexistence remain separate acceptance work. No installed game was
modified or started by this test.
