# Standard configuration migration (layout v2)

This supersedes the phase-one transitional anchor as the current hosted layout.
It is a **one-way** upstream/legacy-to-managed migration. It does not synchronize
back to standalone upstream and never deletes old configs, receipts or logs.

## Targets and activation

Paths are the owner-bound Loader paths v1 roots for `gaku.vrify`:

```text
gakumas-local/
  Configs/gaku.vrify/config.json
  PluginData/gaku.vrify/
    active-layout.json
    migrations/layout-v2.json
    migrations/legacy-config-layout.<source-SHA256>.json
  Logs/gaku.vrify/<session>.log
```

`active-layout.json` is the unique active-layout switch. A copied config alone
does not activate a migration. It is written only after config copy/validation,
source/target fingerprint checks and any available legacy receipt backup.
The new active config and Logs root must match the current Loader roots.
OpenXR dependency discovery still uses the executable/application directory.

A valid new config has priority. Otherwise dedicated
`gakumas-vr/config.json` has priority over known VR fields in
`gakumas-local/localizationConfig.json`. Dedicated copies retain their original
bytes and unknown fields; mixed import copies only the known VR whitelist and
normalizes aliases with new names preferred. Unsupported schemas and malformed
or incorrectly typed known fields block conversion without default replacement.

Existing legacy installs remain in place until the explicit offline apply step.
Fresh installs and already-present new configs establish standard state without
creating an old-directory compatibility anchor. A pending approved migration may
resume during plugin activation. Completed standard layouts always select the
new config; old leftovers are never used as a fallback, even if the new target
is missing or damaged. Older hosts without the paths service fail closed when
the known standard state exists.

Old `gakumas-vr/config-layout.json` is retained byte-for-byte. When readable, it is
also copied to a content-addressed backup under PluginData. An already valid new
config does not depend on reading obsolete legacy data; an unavailable old
receipt is recorded as `not-inspected` and left untouched. Old log files stay in
place; only subsequent managed diagnostics use Logs. Program example/version/
license files in `gakumas-vr` are unchanged by this data migration.

## Read-only plan, backup, explicit apply

The native utility is built at `build/tools/Release/vr-config-migration.exe` and
packaged at `gakumas-local/tools/gaku.vrify/vr-config-migration.exe`. It does not
load the game/plugin/XR runtime or call Python. It currently supports the default
portable root (`<GameRoot>/gakumas-local`), matching the current Loader layout;
it does not guess external overrides or scan other installations.

With `$GameRoot` set to the verified installation and `$PlanPath` set to an
unused JSON filename **outside the installation**:

```powershell
$tool = './build/tools/Release/vr-config-migration.exe'
& $tool --plan $GameRoot $PlanPath
$planHash = (Get-FileHash -LiteralPath $PlanPath -Algorithm SHA256).Hash
# Inspect the plan and back up original config/receipt before this step.
& $tool --apply $PlanPath --expect-sha256 $planHash
```

Planning reads fixed known files and writes only the requested plan, containing
paths/formats/hashes rather than configuration values. Applying requires a
matching plan hash and a stopped game. Changed source data requires a new plan;
an existing or competing target is never overwritten. A new explicit adoption
plan can select an already valid target after inspection.

## Restart and failure recovery

The PluginData journal records `planned`, `staged`, `copied`, `completed` phases
and the approved plan hash. Staging is on the target volume, flushed and hashed.
The journal stores the staged file's volume/file ID before a no-replace rename.
After an interruption it recognizes only that same file ID plus expected hash;
an unrelated identical-content file is preserved and is not silently claimed.

The same approved apply command resumes an unchanged plan. Before activation,
an updated inspected plan may supersede an uncommitted one; old metadata is kept
in `supersededPlans`. Source and receipt originals remain unchanged throughout.
If an active switch was committed before final journal completion, the standard
config is already authoritative. Reapplying finalizes the remaining journal.
A completed journal can reconstruct a missing active pointer from the valid new
target without consulting old data. A missing/invalid committed target fails
closed and requires recovery of the new config from the user's backup.

Failed or interrupted staging can leave small owned temporary files in the
target directory. They are preserved unless ownership is proven; the tool never
deletes an unknown temporary or a competing target to force progress.

The installer's `Restore.ps1` restores **program package files**, including the
tool/example, and preserves active config, PluginData state and all old originals.
It is not a reverse data migration. Do not treat retained old data as synchronized
settings after the new layout has been used.

## Verification boundaries

Synthetic tests inject interruptions at durable phases, source/target races,
same-hash competitors, missing targets, schema/type errors, lock/I/O failures and
long Unicode/space paths. The production utility and DLL also run in separate
synthetic processes. No test launches a real game or migrates real user files.
Back up the selected installation before applying its inspected plan. Synthetic
checks do not certify game startup, headset rendering or controller behavior.
