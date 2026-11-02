# GakuLoader consumer SDK

`GakuPlugin.h` is the unmodified 295-line C ABI header from GakuLoader
`include/GakuPlugin.h`. Source checkpoint:
`6db0f56440b183d37b57c467d68b452d13eb3d47`; the later documentation checkpoint
`c4c302f33b765a45fc81556cf5df276da424a70d` preserves identical header bytes.

SHA256: `c02e006180e464a770bd31df52e89f5d539ceaaf4978f4e056734b5a5ca00abd`.

`GakuPaths.h` is the unmodified optional paths-service declaration from GakuLoader
checkpoint `0426648bd0601486347513af200aa4167a9fa09c`, `include/GakuPaths.h`.
SHA256: `a1182d4903c642299fe28e3687a46ac23c19dfdb40e47a83e84da7af52865ea3`.
That checkpoint preserves identical `GakuPlugin.h` bytes. Paths service v1 adds
owner-bound UTF-16 Configs/UserData/PluginData/Cache/Logs/Temp roots without changing
the existing Loader ABI v1. Both public headers are build inputs; running on an
older host is limited to compatible legacy layouts.

The accompanying LICENSE is the complete original MIT notice, copyright
2026 GakuLoader Contributors. Retain it with copies of the SDK.

This copy provides only public consumer declarations: types, service tables and
plugin exports. It does not include the Loader implementation, its host registry,
hooks, desktop UI implementation, libraries or binary. Plugin builds can use
this directory directly; `-LoaderSdkDirectory` permits an explicitly selected
compatible SDK. Runtime installation still requires a matching GakuLoader host.
