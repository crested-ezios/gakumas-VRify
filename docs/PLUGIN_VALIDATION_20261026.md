# Hosted plugin validation - 2026-10-26

This records the initial `5c9d6ab` artifact. The subsequent enabled VR-less startup
work and current artifact evidence are in
[VRLESS_SMOKE_20261026.md](VRLESS_SMOKE_20261026.md). Hashes below remain historical.

This inventory implementation builds and passes the offline checks below. It has
not been deployed into the user's normal game installation or accepted in a
headset. See [GAKULOADER_PLUGIN.md](GAKULOADER_PLUGIN.md) for usage and the remaining
camera/render coexistence work.

## Built artifact

- Release x64: `build/bin/plugin/x64/Release/GakuVRify.dll`, 4,777,984 bytes.
- SHA256: `9e2865aff345b47b8673daf43aa32eb63d6ebefea6c257d939dff0a50c6a2d0d`.
- Source input fingerprint: `785d198bd5d310c1d32dfa0114175e5756bb7dcdd7d3884ce4849b3210a5b6d2`.
- GakuLoader SDK fingerprint: `76987362c4885ba73d35e11a4c99486809bc9aa0550059f74aceca1c12d7c801`.
- Locked upstream: `318107b6af69740d55f3674ad29644f0b21284e7`.
- Compiler: installed MSVC x64 toolchain through Visual Studio 2026, release CMake
  target. Third-party dependencies came from the locked Conan cache.
- PE exports: exactly `GakuPlugin_Query`; no version proxy/test-host exports.
  No MinHook implementation/import is present. Generated target excludes the proxy,
  old bootstrap/platform/HookManager, nested ModLoader, old desktop GUI and updater.
- `build/plugin-package/` has the plugin, pinned OpenXR loader 1.1.61, independent
  config, version marker, licenses and a fully checked file-hash manifest. It has
  no `version.dll` or Localify configuration.

## Passed checks

| Check | Evidence and limit |
|---|---|
| Production DLL contract/lifecycle | `tests/test-hosted-plugin.ps1`: nine isolated native processes; ABI/output preservation, four required-service failures, service conflict, disabled lifecycle, enabled-without-runtime, malformed config. No game/XR/window was started. |
| Production hook facade | Same runner compiles `HostedHooks.cpp` directly: whole batch forwarding, exact host status, retained originals on create/enable failure, first-failure latch, deduplicated owner-only release. |
| Installer and recovery | `tests/test-plugin-install.ps1`: installs the actual package into an inert fixture, preserves host and Localify/VR config, restores old DLLs, removes newly installed files, refuses recovery over later user edits before changing any file, rejects the unsupported full Unity combination. |
| Source provenance | `tests/test-source-layout.ps1`, upstream boundary and ownership checks; ten `test-source-pipeline.ps1` cases including mode/SDK drift. Expected rejected patch fixtures are part of this suite. |
| Shared camera entry regression | `tests/test-shared-entry-contracts.ps1`: extracted staged hook bodies, original count/order, nested render, exception and early-return paths. This is not live Unity. |
| Existing VR math/input | `test-head-pose-core.ps1`, `test-free-camera-turn.ps1`, `test-pointer-smoother.ps1` passed with the installed MSVC toolchain. |
| Standalone hook manager | `test-hook-manager.ps1` still passes partial create/queue/apply failure and trampoline-lifetime regressions. The full standalone proxy was not rebuilt. |

Local evidence is under `build/plugin-build.log`, `build/hosted-plugin-tests/`,
`build/plugin-install-tests.log`, `build/pipeline-tests.log` and the individual
regression logs. Debug, real Unity/IL2CPP runtime behavior, headset stereo/6DoF,
controller input, scene transitions, XR loss and graceful game exit remain untested.
GakuLocalify/Motion full Unity coexistence, final-pose gaze ordering, Motion's
status-service consumer and a host desktop parameter panel remain future work.

## Preserved local state

The initial ignored upstream cache had existing changes in four ImGui files.
Its complete checkout was preserved at
`.upstream/localify-preserved-20261026-015630/`; a clean checkout at the locked
commit was created separately for staging. No changes were discarded. Relocated
Conan/CMake Windows launchers were repaired using the already pinned requirements.
The `legacy` tree and sibling GakuLoader/GakuMotion repositories were not edited.
