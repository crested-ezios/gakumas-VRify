#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameRoot,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$LoaderSdkDirectory = '',
    [switch]$SkipPackage,
    [switch]$Probe
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
if (-not $LoaderSdkDirectory) {
    $LoaderSdkDirectory = Join-Path $projectRoot 'sdk/gakuloader'
}
if (-not $SkipPackage) {
    & (Join-Path $projectRoot 'package-plugin.ps1') -Configuration $Configuration `
        -LoaderSdkDirectory $LoaderSdkDirectory -Probe:$Probe
}
$packageRoot = Join-Path $projectRoot 'build/plugin-package'
$packageManifest = Join-Path $packageRoot 'plugin-package.json'
if (-not (Test-Path -LiteralPath $packageManifest)) { throw 'Run package-plugin.ps1 first.' }
$manifest = Get-Content -LiteralPath $packageManifest -Raw | ConvertFrom-Json -AsHashtable
if ($manifest.kind -ne 'GakuLoader-plugin' -or $manifest.plugin -ne 'GakuVRify.dll' -or
    $manifest.configuration -ne $Configuration) {
    throw 'Unexpected plugin package identity.'
}
$builtPlugin = Join-Path $projectRoot "build/bin/plugin/x64/$Configuration/GakuVRify.dll"
$sdkHeader = Join-Path $LoaderSdkDirectory 'GakuPlugin.h'
& python (Join-Path $projectRoot 'scripts/source-pipeline.py') verify-binary `
    --binary $builtPlugin --mode Plugin --sdk-header $sdkHeader
if ($LASTEXITCODE -ne 0) { throw 'Refusing to install a stale plugin build.' }
if ((Get-FileHash -LiteralPath $builtPlugin).Hash -ne
    $manifest.files['gakumas-local/plugins/GakuVRify.dll']) { throw 'Package is from a different build.' }
$GameRoot = [IO.Path]::GetFullPath($GameRoot)
if (-not (Test-Path -LiteralPath (Join-Path $GameRoot 'gakumas.exe')) -or
    -not (Test-Path -LiteralPath (Join-Path $GameRoot 'version.dll'))) {
    throw 'Install GakuLoader in a game directory before installing this plugin.'
}
if (Get-Process -Name gakumas -ErrorAction SilentlyContinue) {
    throw 'Close the game before installing GakuVRify.'
}
$configRelative = 'gakumas-vr/config.json'
$configTarget = Join-Path $GameRoot $configRelative
$effectiveConfig = if (Test-Path -LiteralPath $configTarget) { $configTarget } else {
    Join-Path $packageRoot $configRelative
}
$config = Get-Content -LiteralPath $effectiveConfig -Raw | ConvertFrom-Json
if ($config.vrEnabled -and -not $config.vrNativeOnly) {
    foreach ($other in @('GakuLocalify.dll', 'GakuMotion.dll')) {
        if (Test-Path -LiteralPath (Join-Path $GameRoot "gakumas-local/plugins/$other")) {
            throw "Full Unity VR with $other still needs camera/render bridges. Use a separate GakuLoader + VRify test installation."
        }
    }
}

# Validate every input before touching the installation. The loader proxy and
# Localify configuration are deliberately absent from the install whitelist.
$allowed = @('gakumas-local/plugins/GakuVRify.dll', 'openxr_loader.dll', $configRelative,
    'gakumas-vr/version.txt',
    'gakumas-vr/licenses/OpenXR-loader.txt', 'gakumas-vr/licenses/GakuVRify.txt',
    'gakumas-vr/licenses/THIRD_PARTY_NOTICES.md', 'gakumas-vr/licenses/GakuLoader-SDK.txt')
$entries = @()
foreach ($relative in $allowed) {
    $source = Join-Path $packageRoot $relative
    if (-not $manifest.files.ContainsKey($relative) -or -not (Test-Path -LiteralPath $source) -or
        (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $manifest.files[$relative]) {
        throw "Package file verification failed: $relative"
    }
    if ($relative -eq $configRelative -and (Test-Path -LiteralPath $configTarget)) { continue }
    $entries += [pscustomobject]@{ relative = $relative; source = $source }
}
$deploymentRoot = Join-Path $projectRoot ('build/plugin-deployments/' +
    (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $deploymentRoot -Force | Out-Null
$records = @()
foreach ($entry in $entries) {
    $destination = Join-Path $GameRoot $entry.relative
    $existed = Test-Path -LiteralPath $destination
    if ($existed) {
        $backup = Join-Path $deploymentRoot ('backup/' + $entry.relative)
        New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
        Copy-Item -LiteralPath $destination -Destination $backup
    }
    $records += [pscustomobject]@{
        path = $entry.relative
        existed = $existed
        beforeHash = if ($existed) { (Get-FileHash -LiteralPath $destination).Hash } else { $null }
        installedHash = (Get-FileHash -LiteralPath $entry.source).Hash
    }
}
[pscustomobject]@{ gameRoot = $GameRoot; files = $records } |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $deploymentRoot 'deployment.json') -Encoding utf8
@'
#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
if (Get-Process -Name gakumas -ErrorAction SilentlyContinue) { throw 'Close the game before restoring.' }
$record = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Raw | ConvertFrom-Json
# Validate every target before restoring any file. A later plugin/config update
# belongs to the user and must never be overwritten by an older recovery record.
foreach ($file in $record.files) {
    $destination = Join-Path $record.gameRoot $file.path
    if (Test-Path -LiteralPath $destination) {
        $current = (Get-FileHash -LiteralPath $destination).Hash
        if ($current -ne $file.installedHash -and $current -ne $file.beforeHash) {
            throw "Recovery conflict; file changed after installation: $destination"
        }
    } elseif ($file.existed) {
        throw "Recovery conflict; pre-existing file is now missing: $destination"
    }
    if ($file.existed -and (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot ('backup/' + $file.path))).Hash -ne $file.beforeHash) {
        throw "Recovery backup hash mismatch: $($file.path)"
    }
}
foreach ($file in $record.files) {
    $destination = Join-Path $record.gameRoot $file.path
    if ($file.existed) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('backup/' + $file.path)) -Destination $destination -Force
    } elseif (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination
    }
}
Write-Host 'Restored files from before the GakuVRify plugin installation.'
'@ | Set-Content -LiteralPath (Join-Path $deploymentRoot 'Restore.ps1') -Encoding utf8
foreach ($entry in $entries) {
    $destination = Join-Path $GameRoot $entry.relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $entry.source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath $entry.source).Hash) {
        throw "Installed hash mismatch; restore using $deploymentRoot/Restore.ps1"
    }
}
Write-Host "Installed GakuVRify plugin. Existing VR configuration was preserved. Recovery: $deploymentRoot/Restore.ps1"
