# Hosted camera services with missing targets

The hosted Localify camera-render service can be present without owning every
camera target. Previously, VRify interpreted the presence of the service as
ownership of all three entries and skipped its own hooks. Required Unity camera
initialization then failed even though the render-begin hooks had installed.
Failure cleanup closed the camera-begin provider; Motion's subsequent exclusive
render-begin request could encounter the Loader's retained hook lease.

The fix selects ownership per target:

- An absent service or a null published target uses normal exclusive hook
  acquisition. Host lease failures are still honored.
- An exact non-null published target uses the shared observer path.
- Malformed services and non-null mismatches remain fatal.
- Partial ownership subscribes when at least one concrete target is shared;
  directly installed originals establish readiness for the remaining entries.
- Subscription failure is never counted as ready.

The change does not alter Loader retirement or allow one plugin to steal another
plugin's old lease. Restart a process that already failed initialization before
checking the paired fix.

The runtime changes are projected from source checkpoint `b6fe8ac`. This staged
repository retains its own public SDK, build scripts and synthetic fixtures.
No Loader ABI, upstream lock, package configuration or dependency changed.

Run the public regression from the repository root:

```powershell
python -B tests/test-shared-camera-targets.py
```

It compiles the actual production selection and connection functions with MSVC,
using the repository's included SDK. Coverage includes an absent service, all
eight target ownership subsets, non-null mismatches, unresolved metadata,
invalid services, subscription failure and readiness.

The paired inventory build received user confirmation that the reported game
camera behavior recovered on 2026-11-04. That observation is limited to the
reported desktop camera regression; it does not certify a separately compiled
staged DLL, all game scenes, active XR or a complete camera-composition matrix.
