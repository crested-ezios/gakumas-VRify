# Building gakumas-VRify

The default target is the GakuLoader plugin, `GakuVRify.dll`. The public repository includes its Loader ABI header and synthetic test fixtures. A plugin build does not require a private GakuLoader, Localify-provider or Motion implementation checkout.

## Requirements

- Windows x64, PowerShell 7, Git, and Python 3.11 or newer with working `venv` and `pip`.
- Visual Studio 2022 or newer Desktop C++ tools, MSVC x64 toolset 14.40 or newer, and a Windows SDK. The consumer uses the newest installed compatible toolset in the selected VS instance. The script prints that toolset, pins it in CMake, selects the matching VS17/VS18 generator, and generates a Conan profile for that compiler. MSVC 14.53 is the locally validated consumer; other compatible toolchains require their own build validation.
- Network access for the exact Localify upstream, PyPI, Conan packages and, when packaging, the official Khronos OpenXR loader. Existing verified caches may be reused.

A game installation and headset are not needed for compilation or the public synthetic checks. Successful rendering and camera composition require separate runtime validation.

## Build the plugin

Run from PowerShell 7:

```powershell
git clone --branch feat/plugin-host https://github.com/crested-ezios/gakumas-VRify.git
Set-Location gakumas-VRify
./build.ps1 -Mode Plugin -Configuration Release
```

The default SDK is [`sdk/gakuloader/`](sdk/gakuloader/PROVENANCE.md), containing `GakuPlugin.h`, `GakuPaths.h` and their complete MIT license. To test another compatible SDK explicitly, pass `-LoaderSdkDirectory '<sdk include directory>'`. Both headers are build inputs, so rebuild and revalidate the resulting DLL after changing them.

The build script:

1. Verifies/fetches the detached Localify checkout pinned by `upstream.lock.json` into `.upstream/localify/`.
2. Creates `.tools/build/` and installs Conan 2.32.0 and CMake 4.4.3 from `scripts/build-requirements.txt`.
3. Stages upstream `src/` and `deps/` plus this repository's owned adapters into `build/source/`. Patch inputs, digests and output hashes must match `patches/manifest.json`; the fetched upstream remains unchanged.
4. Resolves the Conan lock/profile with the repository-local `.tools/conan2/` cache, configures CMake and builds the plugin.
5. Records source, SDK and binary receipts used to reject stale packaging inputs.

Outputs: **`build/bin/plugin/x64/Release/GakuVRify.dll`**, with a PDB, and **`build/tools/Release/vr-config-migration.exe`**. The migration utility is a native executable with static CRT and does not invoke Python at runtime. The build script does not install or migrate game data. Edit owned sources, then rebuild through `build.ps1`; direct edits to generated `build/source/` are not permanent changes.

Use `-CompilerVersion 14.53.36919` to select an exact installed toolset. Use `-Offline` only with an already available locked upstream, Python tools and Conan binaries; it forbids dependency downloads/rebuilds. It is not a self-contained offline installer. `-DependencyCompilerVersion 194` explicitly selects dependencies built for the older compatible MSVC 19.4 ABI while retaining the actual newer consumer compiler. This is an optional cache-reuse choice, not a claim that the consumer is compiler 19.4. The local validation used `-Offline -DependencyCompilerVersion 194 -CompilerVersion 14.53.36919`; rebuilding the dependencies themselves under 19.5 was not tested.

Use `-CompilerProcesses 2` to reduce memory pressure, or `-Configuration Debug` for a Debug build. Historical Release results do not establish that Debug or a new toolchain has passed. Compiler versions, paths and dependency binaries can affect output hashes; byte-identical DLLs are not promised.

## Public checks

These checks use this repository's sources, the included SDK and generated fixtures:

```powershell
./tests/test-source-pipeline.ps1
./tests/test-camera-provider.ps1
./tests/test-head-pose-core.ps1
```

After building the plugin, run its production-DLL lifecycle and mock-host tests:

```powershell
./tests/test-hosted-plugin.ps1 -Configuration Release
./tests/test-config-migration-tool.ps1
```

The mock host exercises the real plugin DLL and owned hook facade without compiling a private Loader implementation. Source-pipeline tests create artificial upstream repositories; they are not verification of an installed game or the real upstream build tree.

For an already prepared combined source tree, `./scripts/test-upstream-boundary.ps1` and `./tests/test-source-layout.ps1` check the actual pinned/staged-source boundary. These need the upstream checkout and generated staging from the build, rather than only fixture tests.

## Optional maintainer integration

The following tests are research/integration checks, not prerequisites for a normal public plugin build:

| Check | Additional inputs and limits |
| --- | --- |
| `tests/test-vrless-smoke.ps1` with `-RunMaintainerIntegration` | Built plugin, separately supplied GakuLoader implementation/MinHook and Localify provider sources, plus the pinned official loader. It compiles a real host and provider fixture. It does not prove headset rendering. |
| `tests/test-camera-begin.py` with explicit maintainer opt-in | Separately supplied Motion implementation; extracts its actual handshake/callback to compare with the public provider. The Motion core is not bundled here. |
| `tests/vrless/OpenXrAvailabilityProbe.cpp` | Separate manual diagnostic source for an installed OpenXR runtime/device. The smoke runner does not invoke it, and it is not part of default synthetic checks. |

Pass `-GakuLoaderRoot`, `-LocalifyRoot` and `-RealOpenXrLoader` explicitly to the smoke runner as needed. Do not obtain missing integration inputs by copying private core implementations into this repository. Historical integration results and public mock-host results cover different inputs.

## Package and install the plugin

```powershell
./package-plugin.ps1 -Configuration Release
```

This verifies the build receipt, fetches/verifies the official Khronos OpenXR loader 1.1.61 and creates `build/plugin-package/`, including component licenses and a file-hash manifest. Package contents and recovery are described in [the plugin guide](docs/GAKULOADER_PLUGIN.md).

With the verified loader archive already cached, `./package-plugin.ps1 -Configuration Release -Offline` refuses any download. Packages include the native migration utility and `config.example.json`, with no active settings file. Standard directories and explicit migration are described in [layout v2](docs/CONFIG_MIGRATION_V2_20261102.md).

Installation changes a game directory. Close the game first and use the package matched to your Loader and any cooperating providers:

```powershell
./install-plugin.ps1 -Configuration Release -GameRoot '<directory containing gakumas.exe>'
```

`install-to-game.ps1` also defaults to Plugin mode. The generic installer refuses full Unity VR alongside Localify/Motion because it does not validate/upgrade their matching provider pair. Shared bridge implementations do not remove that packaging guard. It preserves existing VR settings and records backups/recovery under `build/plugin-deployments/`.

## Standalone proxy workflow

The original standalone mode is separate from the GakuLoader plugin:

```powershell
./build.ps1 -Mode Standalone -Configuration Release
./package-vr.ps1 -Configuration Release
./install-to-game.ps1 -Mode Standalone -Configuration Release -GameRoot '<directory containing gakumas.exe>'
```

Its output is `build/bin/x64/Release/version.dll`, and its package is `build/vr-package/`. It supplies a proxy rather than a GakuLoader plugin; do not install both proxies together. The upstream Releases refer to this standalone workflow, not a new plugin binary certification.

Standalone retains the original version proxy, bootstrap, MinHook backend and Localify GUI hotkey. It does not load the Plugin-mode DLLs in `gakumas-local/plugins/`; the retired embedded plugin scanner and its API have been removed. The Standalone cpprest consumers use C++20 locally to avoid the incomplete JSON type error in that dependency with current MSVC.

After building Standalone, check the production DLL in an isolated native fixture:

```powershell
./tests/test-proxy-exports.ps1
./tests/test-hook-manager.ps1
./tests/test-standalone-smoke.ps1
```

The smoke check exercises the real proxy/bootstrap, system version forwarding, missing XR loader and worker stop. It also checks configuration bytes and the working directory. It launches only a newly generated fixture, with no real game, Unity metadata or OpenXR session.

## Returning from Plugin to Standalone

Standalone reads `gakumas-local/config.json`, `gakumas-local/localizationConfig.json` and `gakumas-vr/config.json`. It does not read the Loader's managed Configs root or active-layout record, so retained legacy settings may be older than those last saved in Plugin mode.

1. Close the game and back up the managed VR config, any existing `gakumas-vr/config.json`, and the DLLs being replaced.
2. To retain the current VR preferences, explicitly copy the selected managed config (normally `gakumas-local/Configs/gaku.vrify/config.json`) to `gakumas-vr/config.json`. Keep both backups; this selects settings without reverse migration or synchronization.
3. Replace GakuLoader's `version.dll` with the intended Standalone VRify `version.dll` and its matching `openxr_loader.dll`. Use the explicit Standalone commands above for this repository's build. Its installer merges legacy settings and sets diagnostic logging from `-Probe`.

Plugin DLLs, managed configs and PluginData may remain on disk. Standalone saves update the legacy VR config only. Localify settings and translation resources use their legacy locations separately; copying the VR config does not move them. The plugin installer's `Restore.ps1` restores package files and preserves data; it does not select the current Standalone settings.

## Troubleshooting

- Missing SDK: check `sdk/gakuloader/GakuPlugin.h`, or the explicit override directory, before building.
- Upstream mismatch/dirty checkout: preserve intentional work and follow [UPSTREAM.md](UPSTREAM.md); the fetch script does not overwrite a dirty checkout.
- Compiler/profile mismatch: check the selected Visual Studio instance, actual MSVC toolset and the generated `build/vs2022/.../dependency.profile` together; `scripts/vs2022.profile` is only a VS2022 example. A generator name alone does not prove a compatible dependency graph.
- Stale binary receipt: rebuild from owned sources; do not bypass the staging/SDK hash checks.
- A fixture test passing is not a full DLL build, real upstream verification or game/headset acceptance. The dated reports retain those distinctions.
