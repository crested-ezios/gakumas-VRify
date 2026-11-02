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

# New-file install: a subsequent config edit must block the entire old recovery.
Remove-Item -LiteralPath $pluginTarget, $vrConfig
& (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage
$recovery = Latest-Recovery
$installedConfig = [IO.File]::ReadAllBytes($vrConfig)
$installedPluginHash = (Get-FileHash -LiteralPath $pluginTarget).Hash
Set-Content -LiteralPath $vrConfig -Value '{"laterUserEdit":true}'
$refused = $false
try { & $recovery } catch { $refused = $_.Exception.Message -like '*Recovery conflict*' }
Assert $refused 'Recovery overwrote a later user config edit'
Assert ((Get-FileHash -LiteralPath $pluginTarget).Hash -eq $installedPluginHash) 'Recovery mutated files before its conflict preflight'
[IO.File]::WriteAllBytes($vrConfig, $installedConfig)
& $recovery
Assert (-not (Test-Path -LiteralPath $pluginTarget)) 'New plugin was not removed during recovery'
Assert (-not (Test-Path -LiteralPath $vrConfig)) 'New config was not removed during recovery'
Assert ((Get-FileHash -LiteralPath $loaderTarget).Hash -eq $preserved[$loaderTarget]) 'Host proxy was changed'

# Known colliding full-Unity combinations must be rejected before installation.
$motion = Join-Path $localRoot 'plugins/GakuMotion.dll'
Set-Content -LiteralPath $motion -Value 'other plugin sentinel'
$refused = $false
try { & (Join-Path $projectRoot 'install-plugin.ps1') -GameRoot $gameRoot -SkipPackage }
catch { $refused = $_.Exception.Message -like '*camera/render bridges*' }
Assert $refused 'Installer accepted an unsupported full-Unity combination'
Assert (-not (Test-Path -LiteralPath $pluginTarget)) 'Rejected combination installed a plugin'
Write-Host "PASS: real package installation, file preservation, restore, drift refusal and coexistence guard. Fixture: $fixture"
