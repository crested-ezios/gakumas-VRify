# GakuLoader consumer SDK

`GakuPlugin.h` is the unmodified 295-line C ABI header from GakuLoader
`include/GakuPlugin.h`. Source checkpoint:
`6db0f56440b183d37b57c467d68b452d13eb3d47`; the later documentation checkpoint
`c4c302f33b765a45fc81556cf5df276da424a70d` preserves identical header bytes.

SHA256: `c02e006180e464a770bd31df52e89f5d539ceaaf4978f4e056734b5a5ca00abd`.

The accompanying LICENSE is the complete original MIT notice, copyright
2026 GakuLoader Contributors. Retain it with copies of the SDK.

This copy provides only public consumer declarations: types, service tables and
plugin exports. It does not include the Loader implementation, its host registry,
hooks, desktop UI implementation, libraries or binary. Plugin builds can use
this directory directly; `-LoaderSdkDirectory` permits an explicitly selected
compatible SDK. Runtime installation still requires a matching GakuLoader host.
