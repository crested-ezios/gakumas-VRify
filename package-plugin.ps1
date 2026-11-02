#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$LoaderSdkDirectory = '',
    [switch]$Probe,
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($LoaderSdkDirectory)) {
    $LoaderSdkDirectory = Join-Path $projectRoot 'sdk/gakuloader'
}
$sdkHeader = Join-Path $LoaderSdkDirectory 'GakuPlugin.h'
$pluginDll = Join-Path $projectRoot "build/bin/plugin/x64/$Configuration/GakuVRify.dll"
$migrationTool = Join-Path $projectRoot "build/tools/$Configuration/vr-config-migration.exe"
if (-not (Test-Path -LiteralPath $pluginDll -PathType Leaf)) {
    throw "Missing plugin output: $pluginDll. Run ./build.ps1 -Mode Plugin first."
}
& python (Join-Path $projectRoot 'scripts/source-pipeline.py') verify-binary `
    --binary $pluginDll --mode Plugin --sdk-header $sdkHeader
if ($LASTEXITCODE -ne 0) { throw 'Refusing to package a stale or non-plugin binary.' }
& python (Join-Path $projectRoot 'scripts/source-pipeline.py') verify-binary --binary $migrationTool --mode Tool
if ($LASTEXITCODE -ne 0) { throw 'Refusing to package a stale or missing migration tool.' }

# This is the same pinned Khronos runtime loader used by the standalone package.
$toolsRoot = Join-Path $projectRoot '.tools'
$loaderVersion = '1.1.61'
$loaderArchive = Join-Path $toolsRoot "openxr_loader_windows-$loaderVersion.zip"
$loaderRoot = Join-Path $toolsRoot "openxr-loader-$loaderVersion"
$loaderUrl = "https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/download/release-$loaderVersion/openxr_loader_windows-$loaderVersion.zip"
$archiveSha256 = 'CFDCA34B8CB4C2BEF6DEA05273DAFF987E58C97D5767C04F0A20B1029EAABE7F'
$loaderSha256 = '0C262C9384BE6BC82634E03A324E2344CDFF41A4140FA8935891666184E8C596'
$loaderDll = Join-Path $loaderRoot 'x64/bin/openxr_loader.dll'
$loaderLicense = Join-Path $loaderRoot 'share/doc/openxr/LICENSE'
New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null
if (-not (Test-Path -LiteralPath $loaderArchive) -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $loaderArchive).Hash -ne $archiveSha256) {
    if ($Offline) { throw 'Offline packaging requires the verified pinned OpenXR loader archive in .tools.' }
    Invoke-WebRequest -UseBasicParsing -Uri $loaderUrl -OutFile $loaderArchive
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $loaderArchive).Hash -ne $archiveSha256) {
    throw 'Pinned OpenXR loader archive SHA256 mismatch.'
}
if (-not (Test-Path -LiteralPath $loaderDll) -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $loaderDll).Hash -ne $loaderSha256) {
    New-Item -ItemType Directory -Path $loaderRoot -Force | Out-Null
    Expand-Archive -LiteralPath $loaderArchive -DestinationPath $loaderRoot -Force
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $loaderDll).Hash -ne $loaderSha256) {
    throw 'Pinned OpenXR loader DLL SHA256 mismatch.'
}
if (-not (Test-Path -LiteralPath $loaderLicense)) { throw 'OpenXR loader license missing.' }

$buildRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))
$packageRoot = [IO.Path]::GetFullPath((Join-Path $buildRoot 'plugin-package'))
if ([IO.Path]::GetDirectoryName($packageRoot) -ne $buildRoot -or
    [IO.Path]::GetFileName($packageRoot) -ne 'plugin-package') {
    throw "Refusing to replace unexpected package path: $packageRoot"
}
if (Test-Path -LiteralPath $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
$pluginsRoot = Join-Path $packageRoot 'gakumas-local/plugins'
$vrRoot = Join-Path $packageRoot 'gakumas-vr'
$licensesRoot = Join-Path $vrRoot 'licenses'
$toolRoot = Join-Path $packageRoot 'gakumas-local/tools/gaku.vrify'
New-Item -ItemType Directory -Path $pluginsRoot, $vrRoot, $licensesRoot, $toolRoot -Force | Out-Null
Copy-Item -LiteralPath $migrationTool -Destination (Join-Path $toolRoot 'vr-config-migration.exe')
Copy-Item -LiteralPath $pluginDll -Destination (Join-Path $pluginsRoot 'GakuVRify.dll')
Copy-Item -LiteralPath $loaderDll -Destination (Join-Path $packageRoot 'openxr_loader.dll')
Copy-Item -LiteralPath $loaderLicense -Destination (Join-Path $licensesRoot 'OpenXR-loader.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'sdk/gakuloader/LICENSE') -Destination (Join-Path $licensesRoot 'GakuLoader-SDK.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination (Join-Path $licensesRoot 'GakuVRify.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination (Join-Path $licensesRoot 'THIRD_PARTY_NOTICES.md')
$config = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'src/vr/config/defaults.json') | ConvertFrom-Json -AsHashtable
$config['vrDiagnosticsEnabled'] = [bool]$Probe
# The runtime owns first-use discovery and initialization. Shipping an active
# config here would mask legacy mixed VR settings before that discovery runs.
$config | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $vrRoot 'config.example.json') -Encoding utf8
if ($Probe) {
    Write-Warning 'Probe changes the example only. Enable vrDiagnosticsEnabled in the active config after first-use initialization.'
}
$versionSource = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'src/vr/VrVersion.hpp')
$versionMatch = [regex]::Match($versionSource, '#define\s+GAKUMAS_VR_VERSION\s+"(?<version>[^"]+)"')
if (-not $versionMatch.Success) { throw 'GAKUMAS_VR_VERSION not found.' }
[IO.File]::WriteAllText((Join-Path $vrRoot 'version.txt'), $versionMatch.Groups['version'].Value, [Text.UTF8Encoding]::new($false))
$receipt = Get-Content -Raw -LiteralPath ([IO.Path]::ChangeExtension($pluginDll, '.build.json')) | ConvertFrom-Json -AsHashtable
$packageManifest = [ordered]@{
    kind = 'GakuLoader-plugin'
    plugin = 'GakuVRify.dll'
    version = $versionMatch.Groups['version'].Value
    configuration = $Configuration
    sourceInputs = $receipt.inputs
    loaderSdk = $receipt.loaderSdk
    loaderPathsSdk = $receipt.loaderPathsSdk
    upstream = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'upstream.lock.json') | ConvertFrom-Json -AsHashtable
    files = [ordered]@{}
}
foreach ($entry in Get-ChildItem -LiteralPath $packageRoot -File -Recurse | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($packageRoot, $entry.FullName).Replace('\', '/')
    $packageManifest.files[$relative] = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$packageManifest | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $packageRoot 'plugin-package.json') -Encoding utf8
Write-Host "Prepared GakuLoader plugin package: $packageRoot"
Write-Host 'The package contains an example config only; active VR and Localify configuration is preserved.'
