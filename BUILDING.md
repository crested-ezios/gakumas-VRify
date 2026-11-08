# Building gakumas-VRify

The default target is the GakuLoader-hosted plugin `GakuVRify.dll`. The original standalone proxy remains a separate explicit build. These instructions describe the shared public workflow; release and development checkouts have different SDK defaults and some different build options.

## Requirements

- Windows x64, PowerShell 7, Git, and Python 3.11+ with working `venv` and `pip`.
- Visual Studio 2022 or newer with Desktop C++ tools, MSVC x64 and a Windows SDK.
- Access to the locked Localify source and the configured Python, Conan and packaging dependencies, or an already verified cache where supported.

A game installation and headset are not required for compilation or the synthetic checks. Build success, installed files, process loading, actual XR rendering and visual compatibility are separate results.

## Build the plugin

From the VRify repository root:

```powershell
./build.ps1 -Mode Plugin -Configuration Release
./tests/test-hosted-plugin.ps1 -Configuration Release
```

The release source includes public Loader SDK headers under `sdk/gakuloader/`; the development checkout defaults to a sibling `GakuLoader/include/`. Both accept `-LoaderSdkDirectory` for a compatible SDK directory. Use a matching ABI and retain its license notices. The plugin build does not require private Loader or Motion implementation code.

The script fetches or verifies the locked Localify source, prepares its build dependencies, stages the sources, and builds the plugin. Its inputs and receipts are described in [upstream provenance](UPSTREAM.md). The plugin DLL is emitted at `build/bin/plugin/x64/Release/GakuVRify.dll`; the native migration tool is built separately as part of the workflow. Building does not install files or migrate game data.

Both checkouts support `-CompilerProcesses` and `-LoaderSdkDirectory`. The release checkout additionally supports `-CompilerVersion`, `-DependencyCompilerVersion` and `-Offline`; do not pass those release-only options to the development checkout. An offline build requires the verified upstream, tool and dependency caches to be present.

Public synthetic checks include `tests/test-source-pipeline.ps1`, `tests/test-hosted-plugin.ps1`, and the configuration and camera contract tests in `tests/`. Some checks require the corresponding production DLL to be built first. Maintainer integration tests involving real Loader, Localify or Motion sources require those inputs separately. Historical results apply to their recorded builds, not automatically to a new DLL.

## Packaging and fresh installation

`package-plugin.ps1` creates the older `gakumas-local` plugin package and `install-plugin.ps1` installs that old layout. The current native configuration migration tool also targets the old root. **They are not the new-root fresh installer.** The generic installer may refuse full Unity VR alongside Localify/Motion because it cannot verify the complete provider set.

For a new `GakuLoader/` root use the coordinated complete package and follow the [fresh-install guide](docs/FRESH_INSTALL_GAKULOADER.md). It identifies the plugin, OpenXR loader, configuration and logs. Never treat an old package's manifest as proof that it installed files to the new root.

## Standalone proxy workflow

Standalone is a separate mode. From the repository root:

```powershell
./build.ps1 -Mode Standalone -Configuration Release
./package-vr.ps1 -Configuration Release
./install-to-game.ps1 -Mode Standalone -Configuration Release -GameRoot '<directory containing gakumas.exe>'
```

Standalone provides `version.dll` rather than a GakuLoader plugin. It uses its legacy settings, including `gakumas-vr/config.json`, and does not synchronize them with the new managed VRify configuration. Close the game and back up configurations and program files before switching modes. Do not place two competing `version.dll` proxies at the game root. Upstream releases are standalone distributions and do not certify a build of this hosted plugin.

## Troubleshooting

- **SDK unavailable:** check the applicable default SDK directory or the explicit `-LoaderSdkDirectory`.
- **Upstream mismatch or dirty checkout:** preserve intentional work and follow [UPSTREAM.md](UPSTREAM.md); do not bypass source identity checks.
- **Compiler or dependency failure:** inspect the selected Visual Studio toolchain, Conan profile and build output for that checkout.
- **No headset picture:** check the active OpenXR runtime, matching loader DLL, plugin status and VR logs. Rendering and camera coexistence need separate real-device verification.

Component licenses remain in [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md). Historical camera, headset and migration reports retain their own scope and dates.
