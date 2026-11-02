#Requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $projectRoot ('build/plugin-install-tests/' + [guid]::NewGuid().ToString('N'))
$gameRoot = Join-Path $fixture 'game'
$localRoot = Join-Path $gameRoot 'gakumas-local'
$pluginTarget = Join-Path $localRoot 'plugins/GakuVRify.dll'
$xrTarget = Join-Path $gameRoot 'openxr_loader.dll'
$vrConfig = Join-Path $gameRoot 'gakumas-vr/config.json'
$newConfig = Join-Path $localRoot 'Configs/gaku.vrify/config.json'
$exampleConfig = Join-Path $gameRoot 'gakumas-vr/config.example.json'
$layoutRecord = Join-Path $gameRoot 'gakumas-vr/config-layout.json'
$activeLayout = Join-Path $localRoot 'PluginData/gaku.vrify/active-layout.json'
$toolTarget = Join-Path $localRoot 'tools/gaku.vrify/vr-config-migration.exe'
$mixedConfig = Join-Path $localRoot 'localizationConfig.json'
$loaderTarget = Join-Path $gameRoot 'version.dll'
New-Item -ItemType Directory -Path (Join-Path $localRoot 'plugins'), (Split-Path -Parent $vrConfig) -Force | Out-Null
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Latest-Recovery {
    $record = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build/plugin-deployments') -Filter deployment.json -Recurse |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json).gameRoot -eq $gameRoot } |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    Assert ([bool]$record) 'Installation recovery record missing'
    return Join-Path $record.DirectoryName 'Restore.ps1'
}
function Assert-InstallRefused([string]$Expected) {
    $refused = $false
    try { & (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage }
    catch { $refused = $_.Exception.Message -like $Expected }
    Assert $refused "Installer did not refuse with: $Expected"
    Assert (-not (Test-Path -LiteralPath $pluginTarget)) 'Rejected installation wrote the plugin'
}
$packageRoot = Join-Path $projectRoot 'build/plugin-package'
$package = Get-Content -LiteralPath (Join-Path $packageRoot 'plugin-package.json') -Raw | ConvertFrom-Json -AsHashtable
Assert (-not $package.files.Contains('gakumas-vr/config.json')) 'Package contains an active VR config'
Assert ($package.files.Contains('gakumas-vr/config.example.json')) 'Package example config is missing'
Assert ($package.files.Contains('gakumas-local/tools/gaku.vrify/vr-config-migration.exe')) 'Package native migration tool is missing'
Assert (-not (Test-Path -LiteralPath (Join-Path $packageRoot 'gakumas-vr/config.json'))) 'Package has a stray active config'
Set-Content -LiteralPath (Join-Path $gameRoot 'gakumas.exe') -Value 'fixture only, never executed'
Set-Content -LiteralPath $loaderTarget -Value 'host proxy sentinel'
Set-Content -LiteralPath $pluginTarget -Value 'old plugin sentinel'
Set-Content -LiteralPath $xrTarget -Value 'old OpenXR sentinel'
Set-Content -LiteralPath $vrConfig -Value '{"vrEnabled":false,"userSentinel":17}'
Set-Content -LiteralPath (Join-Path $localRoot 'config.json') -Value '{"userSentinel":"program"}'
Set-Content -LiteralPath (Join-Path $localRoot 'localizationConfig.json') -Value '{"userSentinel":"translation"}'
$preserved = @{}
foreach ($path in @($loaderTarget, $vrConfig, (Join-Path $localRoot 'config.json'),
    (Join-Path $localRoot 'localizationConfig.json'))) { $preserved[$path] = (Get-FileHash -LiteralPath $path).Hash }
$oldPlugin = (Get-FileHash -LiteralPath $pluginTarget).Hash
$oldXr = (Get-FileHash -LiteralPath $xrTarget).Hash
& (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage
foreach ($path in $preserved.Keys) { Assert ((Get-FileHash -LiteralPath $path).Hash -eq $preserved[$path]) "Existing file changed: $path" }
$production = Join-Path $projectRoot 'build/bin/plugin/x64/Release/GakuVRify.dll'
Assert ((Get-FileHash -LiteralPath $pluginTarget).Hash -eq (Get-FileHash -LiteralPath $production).Hash) 'Installed plugin differs from production'
$recovery = Latest-Recovery
& $recovery
Assert ((Get-FileHash -LiteralPath $pluginTarget).Hash -eq $oldPlugin) 'Old plugin was not restored'
Assert ((Get-FileHash -LiteralPath $xrTarget).Hash -eq $oldXr) 'Old OpenXR loader was not restored'
Assert (-not (Test-Path -LiteralPath $toolTarget)) 'New tool program file was retained by install recovery'

# New-file install never initializes active configuration. Recovery covers only
# package files; edits to its example must still block the entire old recovery.
Remove-Item -LiteralPath $pluginTarget, $vrConfig
& (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage
$recovery = Latest-Recovery
Assert (-not (Test-Path -LiteralPath $vrConfig)) 'Installer initialized the legacy active config'
Assert (-not (Test-Path -LiteralPath $newConfig)) 'Installer initialized the paths-v1 active config'
$installedExample = [IO.File]::ReadAllBytes($exampleConfig)
$installedPluginHash = (Get-FileHash -LiteralPath $pluginTarget).Hash
Set-Content -LiteralPath $exampleConfig -Value '{"laterUserEdit":true}'
$refused = $false
try { & $recovery } catch { $refused = $_.Exception.Message -like '*Recovery conflict*' }
Assert $refused 'Recovery overwrote a later package-file edit'
Assert ((Get-FileHash -LiteralPath $pluginTarget).Hash -eq $installedPluginHash) 'Recovery mutated files before its conflict preflight'
[IO.File]::WriteAllBytes($exampleConfig, $installedExample)
# A config created after installation is runtime/user data, not package-owned.
Set-Content -LiteralPath $vrConfig -Value '{"laterUserEdit":true}'
$activeHash = (Get-FileHash -LiteralPath $vrConfig).Hash
& $recovery
Assert (-not (Test-Path -LiteralPath $pluginTarget)) 'New plugin was not removed during recovery'
Assert (-not (Test-Path -LiteralPath $exampleConfig)) 'New example was not removed during recovery'
Assert ((Get-FileHash -LiteralPath $vrConfig).Hash -eq $activeHash) 'Recovery changed runtime/user configuration'
Assert ((Get-FileHash -LiteralPath $loaderTarget).Hash -eq $preserved[$loaderTarget]) 'Host proxy was changed'
Remove-Item -LiteralPath $vrConfig

# Known colliding full-Unity combinations must be rejected before installation.
$motion = Join-Path $localRoot 'plugins/GakuMotion.dll'
Set-Content -LiteralPath $motion -Value 'other plugin sentinel'
Assert-InstallRefused '*camera/render bridges*'

# Mixed legacy values affect compatibility preflight but are never copied or
# changed by installation. The new native-only name wins over the old alias.
Set-Content -LiteralPath $mixedConfig -Value '{"vrEnabled":true,"vrProbeOnly":false,"translationSentinel":17}'
$mixedHash = (Get-FileHash -LiteralPath $mixedConfig).Hash
Assert-InstallRefused '*camera/render bridges*'
Assert ((Get-FileHash -LiteralPath $mixedConfig).Hash -eq $mixedHash) 'Mixed VR source changed on refusal'
Set-Content -LiteralPath $mixedConfig -Value '{"vrEnabled":true,"vrNativeOnly":false,"vrProbeOnly":true}'
Assert-InstallRefused '*camera/render bridges*'
Set-Content -LiteralPath $mixedConfig -Value '{"vrEnabled":true,"vrNativeOnly":true,"vrProbeOnly":false}'
$mixedHash = (Get-FileHash -LiteralPath $mixedConfig).Hash
& (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage
Assert (-not (Test-Path -LiteralPath $vrConfig)) 'Installer masked mixed VR source with a new dedicated config'
Assert ((Get-FileHash -LiteralPath $mixedConfig).Hash -eq $mixedHash) 'Mixed VR source was modified'
& (Latest-Recovery)

# An existing new-layout config is authoritative over both legacy sources.
New-Item -ItemType Directory -Path (Split-Path -Parent $newConfig) -Force | Out-Null
Set-Content -LiteralPath $newConfig -Value '{"schemaVersion":1,"vrEnabled":false,"newSentinel":19}'
Set-Content -LiteralPath $vrConfig -Value '{"vrEnabled":true,"vrNativeOnly":false}'
$newHash = (Get-FileHash -LiteralPath $newConfig).Hash
$oldHash = (Get-FileHash -LiteralPath $vrConfig).Hash
& (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage
Assert ((Get-FileHash -LiteralPath $newConfig).Hash -eq $newHash) 'New-layout configuration changed'
Assert ((Get-FileHash -LiteralPath $vrConfig).Hash -eq $oldHash) 'Legacy dedicated configuration changed'
& (Latest-Recovery)
Set-Content -LiteralPath $newConfig -Value '{"schemaVersion":2,"vrEnabled":false}'
Assert-InstallRefused '*Unsupported configuration schema*'
Set-Content -LiteralPath $newConfig -Value '{"schemaVersion":1,"vrEnabled":"false"}'
Assert-InstallRefused '*Expected a boolean*'
Set-Content -LiteralPath $newConfig -Value '{ broken json'
Assert-InstallRefused '*'

# The marker is an identity check. No arbitrary saved external path is opened,
# and an established paths-v1 installation must not silently use old remnants.
Set-Content -LiteralPath $newConfig -Value '{"schemaVersion":1,"vrEnabled":false}'
@{ schemaVersion = 1; layout = 'paths-v1'; phase = 'completed'; configPath = (Join-Path $fixture 'external/config.json') } |
    ConvertTo-Json | Set-Content -LiteralPath $layoutRecord
Assert-InstallRefused '*Loader path override*'
@{ schemaVersion = 1; layout = 'paths-v1'; phase = 'completed'; configPath = $newConfig } |
    ConvertTo-Json | Set-Content -LiteralPath $layoutRecord
Remove-Item -LiteralPath $newConfig
Assert-InstallRefused '*paths-v1 configuration is missing*'

# Standard committed state outranks the obsolete compatibility anchor. Install
# recovery is program-only and must retain active config/state and old originals.
Set-Content -LiteralPath $newConfig -Value '{"schemaVersion":1,"vrEnabled":false}'
Set-Content -LiteralPath $layoutRecord -Value '{obsolete broken receipt'
New-Item -ItemType Directory -Path (Split-Path -Parent $activeLayout) -Force | Out-Null
@{schemaVersion=1;layoutVersion=2;pluginId='gaku.vrify';configPath=$newConfig;logRoot=(Join-Path $localRoot 'Logs/gaku.vrify')} |
    ConvertTo-Json | Set-Content -LiteralPath $activeLayout -Encoding utf8
$activeHash=(Get-FileHash -LiteralPath $activeLayout).Hash
$obsoleteHash=(Get-FileHash -LiteralPath $layoutRecord).Hash
& (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage
Assert (Test-Path -LiteralPath $toolTarget) 'Migration tool not installed'
& (Latest-Recovery)
Assert ((Get-FileHash -LiteralPath $activeLayout).Hash -eq $activeHash) 'Program recovery changed active layout'
Assert ((Get-FileHash -LiteralPath $layoutRecord).Hash -eq $obsoleteHash) 'Program recovery changed old receipt'
Remove-Item -LiteralPath $newConfig
Assert-InstallRefused '*Standard VR configuration is missing*'
Set-Content -LiteralPath $newConfig -Value '{"schemaVersion":1,"vrEnabled":false}'
$activeDocument=Get-Content -LiteralPath $activeLayout -Raw | ConvertFrom-Json
$activeDocument.layoutVersion=3
$activeDocument | ConvertTo-Json | Set-Content -LiteralPath $activeLayout -Encoding utf8
Assert-InstallRefused '*Unsupported or mismatched standard VR layout*'
Write-Host "PASS: package-only installation, config/state preservation, restore, precedence and standard-layout identity. Fixture: $fixture"
