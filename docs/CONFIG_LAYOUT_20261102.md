# Hosted VRify first-use configuration

This describes the phase-one compatibility layout. The current managed layout
and explicit existing-install migration are specified in
[layout v2](CONFIG_MIGRATION_V2_20261102.md). Existing installations are not
automatically relocated.

## Selection and compatibility

The descriptor id remains `gaku.vrify`. The optional `gakuloader.paths` v1 service
returns absolute UTF-16 Configs and Logs roots for that owner. The existing ABI
v1 structures are unchanged; compiling now requires the matching `GakuPaths.h`
SDK header, while running on an older host remains supported for legacy layouts.

Selection occurs before VR startup and before creating defaults:

1. A valid `Configs/gaku.vrify/config.json` from the Loader service takes priority.
2. An existing `gakumas-vr/config.json` continues to be read and saved in place.
3. If only the same installation's `gakumas-local/localizationConfig.json` has
   known VR fields, those fields are imported once into the old dedicated path.
   The mixed file is retained byte-for-byte. Localify resources and non-VR
   settings are never copied or changed.
4. A fresh install initializes from the release `defaults.json` embedded at
   compile time, using the new Configs root when available, otherwise the legacy
   path. This deliberately preserves the release template's enabled/stereo
   policy rather than accidentally adopting the C++ safe/disabled defaults.

The new names `vrDiagnosticsEnabled` and `vrNativeOnly` win over their old aliases
`vrDebugEnabled` and `vrProbeOnly`. Dedicated settings remain a complete source;
missing keys are not patched from a stale mixed file. Only known VR fields are
imported from mixed settings. Missing schemaVersion and versions 0/1 are accepted
for the known legacy formats; malformed objects, wrong known-field types and
future schemas fail closed without default replacement or fallback to older data.

The startup package includes **config.example.json**, not an active config file.
Neither extraction nor `install-plugin.ps1` creates or overwrites active settings.
The example is documentation; changing it does not change the embedded defaults.
`-Probe` changes the example only. Enable `vrDiagnosticsEnabled` in the selected
active configuration after first initialization to request detailed logging.

## Initialization record and retry

`gakumas-vr/config-layout.json` is the fixed compatibility discovery point. It
records layout (`legacy`/`paths-v1`), operation, converter version, phase,
source/target SHA-256 and target path identity. It is not a pointer used to read
arbitrary locations. Once a paths layout has been selected, a host without the
paths service, a changed root or a missing initialized target disables VR instead
of reading residual legacy settings.
Before a marker exists, an older host also refuses legacy fallback when the
known same-install `gakumas-local/Configs/gaku.vrify/config.json` already exists.
This old-directory marker is an intentional transitional compatibility anchor;
the first phase has not fully consolidated every file into the new categories.

Initialization uses an exclusive process lock and a planned/completed journal.
It validates before committing, creates a unique same-directory temporary file,
flushes it and commits without replacing an existing target. Original mixed data
is retained. Interrupted commits are recognized by the planned target hash;
matching content completes the journal, changed target content is preserved as a
conflict. I/O errors retain the plan and retry next start. If the target is still
absent and the same supported initialization operation has valid changed inputs,
the next start safely replans and retains the old plan fingerprints in
`supersededPlans`. Unrelated Localify edits therefore cannot permanently block an
uncommitted VR import. A changed source kind or an already adopted target that
has disappeared is preserved as a conflict; there is no repeated synchronization.

Regular menu saves return a real success/failure result, use a unique temporary
file and replace only the selected config. Saving after a failed initialization
is refused. A default initialization is reported as `initialized`, a real mixed
import as `migrated-mixed`; neither can report success before durable commit.

New managed layouts may put detailed diagnostics in `Logs/gaku.vrify`. Existing
layouts keep their old log directory. `applicationDirectory` continues to locate
the app-local OpenXR DLL; `logDirectory` is separate. Log retention and relocating
existing data are later phases.

## Local verification

Run the native synthetic startup tests, build the plugin, then the production DLL
lifecycle and synthetic installation tests:

```powershell
./tests/test-config-startup.ps1
./build.ps1 -Mode Plugin -Configuration Release
./tests/test-hosted-plugin.ps1
./package-plugin.ps1 -Configuration Release
./tests/test-plugin-install.ps1
```

The tests use generated small configurations and synthetic installation trees;
they never launch the game or migrate real user files. Actual cold start,
headset rendering/input and game exit remain separate acceptance work. There is
no reverse migration back to standalone upstream and no deployment in this phase.
