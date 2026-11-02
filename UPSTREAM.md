# Upstream and source ownership

Localify upstream: https://git.chinosk6.cn/chinosk/gkms-localify-dmm.git

The authoritative pin is [upstream.lock.json](upstream.lock.json): commit `318107b6af69740d55f3674ad29644f0b21284e7`, tree `ea9ed8fa297f53ec348e133e6c34ec0f41a41822`.

| Location | Ownership and purpose |
| --- | --- |
| `src/vr/` | VR runtime, OpenXR/D3D11 rendering, input, settings and Localify integration adapters |
| `src/host/`, `src/hooks/` | Hosted/standalone integration, shared camera contracts and owned hook adapters |
| `sdk/gakuloader/` | Public Loader ABI header, complete MIT license and provenance; no Loader core implementation |
| `deps/openxr/` | Minimal Khronos OpenXR header subset with its license notices |
| `scripts/`, root PowerShell scripts | Fetch, source staging, build, package and installation workflow |
| `tests/` | Public fixtures/mock-host checks and explicitly optional maintainer integration harnesses |
| `docs/`, `docs/evidence/` | Usage and technical summaries with tested versions, hashes and evidence limits |
| `.upstream/localify/` | Ignored, pinned upstream checkout; do not edit it in place |
| `build/source/` | Ignored combined build input regenerated from upstream and owned sources |

The public integration includes the shared camera/render and camera-begin interfaces required by the cooperating components. Their presence does not bundle the private Loader, Localify provider or Motion core implementation. Normal plugin compilation uses the included ABI SDK; source-level cross-component tests are separate maintainer checks.

Game assets, extracted model/motion data, credentials, private implementation trees, machine configuration and generated build products are not distributed here. Dated validation summaries retain technical conclusions, source/artifact hashes and their limits. Machine-specific paths, ephemeral process/device identifiers and repeated raw logs are omitted from the public summaries. Controller documentation illustrations keep their separate supplied terms.

## Verified source staging

The pinned raw source is staged and receives the exact patches in `patches/manifest.json`. CMake compiles the staged upstream implementations and this repository's owned adapters. ImGui uses the same patch pipeline. Invalid preimages/postimages, source ownership collisions and stale receipts stop build, packaging and installation; do not replace whole upstream implementations to bypass those checks.

See [patches/README.md](patches/README.md) for the pipeline and named seam contracts. `scripts/check-source-ownership.py` checks candidate files for forbidden upstream trees, retired copies and matching long upstream function bodies. Generated source remains ignored. Included SDK provenance is recorded in [sdk/gakuloader/PROVENANCE.md](sdk/gakuloader/PROVENANCE.md).

## Updating upstream

1. Review changes between the current pin and candidate commit, especially each path named by the patch manifest.
2. Port integration behavior into owned adapters; do not use edits to the fetched checkout as the permanent fix.
3. Update the full commit/tree pair in `upstream.lock.json`. Review and regenerate the affected patch digests without bypassing a mismatch.
4. Preserve useful local changes before replacing a generated checkout, then run `scripts/fetch-upstream.ps1` to obtain/verify the chosen pin.
5. Run the boundary/source-layout checks, relevant public tests, a Release build and packaging. Record separate hardware acceptance when changing runtime behavior.

Keep generated dependency checkouts and private component implementations outside this repository. Build and test commands are in [BUILDING.md](BUILDING.md); licenses remain governed by [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the original component terms.
