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
$exampleRelative = 'gakumas-vr/config.example.json'
function Read-JsonObject([string]$Path) {
    $document = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
    if ($document -isnot [System.Collections.IDictionary]) {
        throw "Expected a JSON object; no files were installed: $Path"
    }
    return $document
}
function Assert-KnownSchema($Document, [string]$Path) {
    if ($Document.Contains('schemaVersion') -and
        (($Document.schemaVersion -isnot [int] -and $Document.schemaVersion -isnot [long]) -or
         $Document.schemaVersion -lt 0 -or $Document.schemaVersion -gt 1)) {
        throw "Unsupported configuration schema; no files were installed: $Path"
    }
}
function Read-VrBoolean($Document, [string]$Name, [bool]$Default, [string]$Path) {
    if (-not $Document.Contains($Name)) { return $Default }
    if ($Document[$Name] -isnot [bool]) {
        throw "Expected a boolean '$Name'; no files were installed: $Path"
    }
    return $Document[$Name]
}
$templatePath = Join-Path $packageRoot $exampleRelative
$template = Read-JsonObject $templatePath
Assert-KnownSchema $template $templatePath
$newConfig = Join-Path $GameRoot 'gakumas-local/Configs/gaku.vrify/config.json'
$oldConfig = Join-Path $GameRoot 'gakumas-vr/config.json'
$mixedConfig = Join-Path $GameRoot 'gakumas-local/localizationConfig.json'
$layoutPath = Join-Path $GameRoot 'gakumas-vr/config-layout.json'
$activeLayoutPath = Join-Path $GameRoot 'gakumas-local/PluginData/gaku.vrify/active-layout.json'
$pathsLayout = $false
if (Test-Path -LiteralPath $activeLayoutPath) {
    $active = Read-JsonObject $activeLayoutPath
    $expectedLogRoot = Join-Path $GameRoot 'gakumas-local/Logs/gaku.vrify'
    if (($active.schemaVersion -isnot [int] -and $active.schemaVersion -isnot [long]) -or
        ($active.layoutVersion -isnot [int] -and $active.layoutVersion -isnot [long]) -or
        $active.schemaVersion -ne 1 -or $active.layoutVersion -ne 2 -or $active.pluginId -cne 'gaku.vrify' -or
        $active.configPath -isnot [string] -or $active.logRoot -isnot [string] -or
        -not [IO.Path]::IsPathFullyQualified($active.configPath) -or
        -not [IO.Path]::IsPathFullyQualified($active.logRoot) -or
        [IO.Path]::GetFullPath($active.configPath) -ne [IO.Path]::GetFullPath($newConfig) -or
        [IO.Path]::GetFullPath($active.logRoot) -ne [IO.Path]::GetFullPath($expectedLogRoot)) {
        throw 'Unsupported or mismatched standard VR layout; active state and original files were preserved.'
    }
    if (-not (Test-Path -LiteralPath $newConfig -PathType Leaf)) {
        throw 'Standard VR configuration is missing. Legacy remnants will not be used.'
    }
    $pathsLayout = $true
} elseif (Test-Path -LiteralPath $layoutPath) {
    $layout = Read-JsonObject $layoutPath
    if (($layout.schemaVersion -isnot [int] -and $layout.schemaVersion -isnot [long]) -or
        $layout.schemaVersion -ne 1 -or $layout.layout -cnotin @('legacy', 'paths-v1') -or
        $layout.phase -cnotin @('planned', 'completed') -or $layout.configPath -isnot [string]) {
        throw "Unsupported VR layout record; no files were installed: $layoutPath"
    }
    $pathsLayout = $layout.layout -ceq 'paths-v1'
    if ($pathsLayout) {
        # A saved external override is an identity check, never an instruction
        # for this installer to read arbitrary paths or guess Loader settings.
        if (-not [IO.Path]::IsPathFullyQualified([string]$layout.configPath) -or
            [IO.Path]::GetFullPath([string]$layout.configPath) -ne [IO.Path]::GetFullPath($newConfig)) {
            throw 'VR uses a Loader path override. Use the matching Loader path configuration to inspect compatibility before installing.'
        }
        if (-not (Test-Path -LiteralPath $newConfig -PathType Leaf)) {
            throw 'VR paths-v1 configuration is missing. Resolve its first-use state before installation; legacy remnants will not be used.'
        }
    }
}
$effectiveConfig = $templatePath
$config = $template
if (Test-Path -LiteralPath $newConfig) {
    $effectiveConfig = $newConfig
    $config = Read-JsonObject $newConfig
} elseif (-not $pathsLayout -and (Test-Path -LiteralPath $oldConfig)) {
    $effectiveConfig = $oldConfig
    $config = Read-JsonObject $oldConfig
} elseif (-not $pathsLayout -and (Test-Path -LiteralPath $mixedConfig)) {
    $mixed = Read-JsonObject $mixedConfig
    $knownVrKeys = @($template.Keys | Where-Object { $_ -ne 'schemaVersion' }) + @('vrDebugEnabled', 'vrProbeOnly')
    if (@($mixed.Keys | Where-Object { $_ -cin $knownVrKeys }).Count -gt 0) {
        $effectiveConfig = $mixedConfig
        $config = $mixed
    }
}
Assert-KnownSchema $config $effectiveConfig
# Missing legacy keys use the runtime's safe defaults; pure new installs use
# the release template selected above. New names take precedence over aliases.
$vrEnabled = Read-VrBoolean $config 'vrEnabled' $false $effectiveConfig
$nativeKey = if ($config.Contains('vrNativeOnly')) { 'vrNativeOnly' } else { 'vrProbeOnly' }
$vrNativeOnly = Read-VrBoolean $config $nativeKey $true $effectiveConfig
if ($vrEnabled -and -not $vrNativeOnly) {
    foreach ($other in @('GakuLocalify.dll', 'GakuMotion.dll')) {
        if (Test-Path -LiteralPath (Join-Path $GameRoot "gakumas-local/plugins/$other")) {
            throw "Full Unity VR with $other still needs camera/render bridges. Use a separate GakuLoader + VRify test installation."
        }
    }
}

# Validate every input before touching the installation. The loader proxy and
# Localify configuration are deliberately absent from the install whitelist.
$allowed = @('gakumas-local/plugins/GakuVRify.dll', 'gakumas-local/tools/gaku.vrify/vr-config-migration.exe',
    'openxr_loader.dll', $exampleRelative,
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
Write-Host "Installed GakuVRify plugin and example config. Active configuration was preserved; first-use import runs in VRify. Recovery: $deploymentRoot/Restore.ps1"
