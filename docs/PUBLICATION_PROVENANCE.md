# Publication source and history

This branch is the complete hosted-plugin integration prepared for publication
in the existing public fork. Its public parent is
`562bf40c341758c205a18104f4d254247b5ea058` on `feat/plugin-host`.
The full functional source checkpoint is
`4d9b684499aeb9040736852b02ab4f9a2d6d84fc`, tree
`bc61c976bb8a1e8ec73c75933adbf922328b4718`.

The publication commit applies the complete checkpoint tree to that existing
public parent, then adds the public SDK/build/test changes and edits described
below. It is a normal new commit whose parent is already public. The six original
integration commits are retained in the local source archive/ref only; they are
not publication ancestors. Publishing this branch does not publish those raw
historical log versions, rewrite the public fork, merge into `main`, or change
the default branch.

## Original integration checkpoints

These identifiers describe the functional source provenance, not commits that
must be pushed as publication history:

```text
5c9d6ab9849e25af1e3ebdba988b679c01bf77f5 feat: add GakuLoader-hosted VRify plugin and offline validation
b6bb39eb14c15a1cc911be924c74403236bf5643 test: verify VR-less startup with real host and explicit XR availability
de727ed3acda64b5bbafade43e84657a1f9c2c68 fix: join Localify camera render bridge before starting XR
5ff5b91d75d1ec1d2861d2253467c668c4b4844e Share camera begin boundaries with Motion without exclusive hook conflicts
d466084f237373dd6e57acf485c5d58a268062c9 Record paired camera coexistence deployment awaiting game acceptance
4d9b684499aeb9040736852b02ab4f9a2d6d84fc Record non-VR camera acceptance and deferred VR camera scheduling issue
```

## Preparation changes

- README in four languages, build instructions and hosted-plugin usage describe
  the current plugin target, installed SDK, public checks and runtime limits.
- The dated camera reports and evidence JSON retain technical outcomes, artifact
  hashes and acceptance boundaries. Redundant log blocks, process IDs, temporary
  device identifiers, module inventories and machine-local absolute paths were
  replaced by technical summaries. Historical reports do not certify new DLLs.
- `sdk/gakuloader/GakuPlugin.h` is the exact consumer C ABI header, accompanied
  by its complete original MIT notice and [provenance](../sdk/gakuloader/PROVENANCE.md).
  No Loader registry, hook engine, UI implementation, Motion implementation or
  Localify provider implementation is copied into this source tree. The existing
  three camera/service headers are consumer/service declarations, and the VRify
  camera-begin provider is VRify's own implementation.
- The build selects and pins an installed MSVC toolset, generates its actual Conan
  compiler profile, and permits an explicit compatible older dependency ABI for
  cache reuse. `-Offline` uses only available tools/upstream/packages. The default
  build does not depend on private sibling source directories.
- Public camera-provider tests directly exercise the owned provider using only
  the included ABI. Existing real-DLL lifecycle/facade tests use a synthetic mock
  host. Cross-component production-host/Motion tests require explicit maintainer
  opt-in and separately supplied implementation trees.
- Plugin packages retain the SDK's original MIT license as `GakuLoader-SDK.txt`.

Pinned Localify upstream source remains a separate build dependency described in
[UPSTREAM.md](../UPSTREAM.md) and `upstream.lock.json`; the public checkout is not
falsely represented as a vendored complete upstream snapshot.

## Runtime scope

This preparation preserves the full checkpoint's Localify camera-render consumer,
VRify diagnostics/service, and Motion camera-begin provider. Manual Motion camera
movement/rotation without VR was accepted for the historical paired build. With VR
active, Motion camera control remains a Known Issue. Compilation and synthetic
checks do not establish successful headset rendering or general camera composition.
See [the integration guide](GAKULOADER_PLUGIN.md) and dated reports for limits.

## Managed configuration and dual-mode maintenance

The managed-configuration refresh at `ce7cdba63f352bea230bf5beb3adcfc336720264`
projects source checkpoint `e6b17a21c94b30e2d94a925b5dcb45c742f19b26`, including
`b50c97ad5c459b7372ac188e868e43186ef867fd` and the layout-v2 migration. It retains
the public SDK, compiler selection, offline build and packaging adaptations.

The subsequent maintenance source is
`3d51912ba73307e078473603b70588ab8b631686`. It removes the retired embedded
ModLoader/API and dispatch sites while retaining the original Standalone proxy,
bootstrap and MinHook backend. `VrBootstrap.cpp` and `HookManager.cpp` remain
identical to the pre-hosted `c353d1a847635f69a2547c55c146614962b012fe` baseline.
Five Standalone cpprest consumers use a local C++20 compiler option; repeated XR
availability retries notify the host only when the unavailable reason changes.
The added Standalone fixture and retry assertion are synthetic checks, with the
same runtime and headset limits described above.

This preparation keeps the public build/test entry points, installed SDK default,
component notices and migration protocol. It does not import internal handoffs,
raw logs, dependency caches or the separate historical private candidate's review
metadata. `SOURCE_MANIFEST.json` maps this maintenance projection and its public
adaptations to the prior staged checkpoint.
