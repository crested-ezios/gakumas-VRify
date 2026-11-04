# GakuLoader consumer SDK

`GakuPlugin.h` is the unmodified 298-line C ABI header from GakuLoader
`include/GakuPlugin.h`. Source checkpoint:
`ed5ad70819907c0a0f5acc25e804b0aea205fa57`, using its canonical LF Git blob.

SHA256: `12f4e4db22ff0505990b66cfeaf97b1cae54486c9eb415e7efc3adfd9984867e`.
Relative to the previously included SDK, it appends two appearance-theme enum
values and clarifies hotkey identity comments; ABI v1 structures and signatures
are unchanged.

`GakuPaths.h` is the unmodified optional paths-service declaration from GakuLoader
checkpoint `ed5ad70819907c0a0f5acc25e804b0aea205fa57`, `include/GakuPaths.h`.
SHA256: `a1182d4903c642299fe28e3687a46ac23c19dfdb40e47a83e84da7af52865ea3`.
It is byte-identical to the earlier `0426648` paths header. Paths service v1 adds
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
