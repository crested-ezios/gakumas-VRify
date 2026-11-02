#Requires -Version 7.0
[CmdletBinding()]
param([string]$ToolPath='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(-not $ToolPath) { $ToolPath=Join-Path $projectRoot 'build/tools/Release/vr-config-migration.exe' }
$fixture=Join-Path $projectRoot ('build/config-migration-tool-tests/'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $fixture 'game 中文 space'
$oldConfig=Join-Path $game 'gakumas-vr/config.json'
$oldReceipt=Join-Path $game 'gakumas-vr/config-layout.json'
$newConfig=Join-Path $game 'gakumas-local/Configs/gaku.vrify/config.json'
$active=Join-Path $game 'gakumas-local/PluginData/gaku.vrify/active-layout.json'
New-Item -ItemType Directory -Path (Split-Path -Parent $oldConfig) -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $game 'gakumas.exe'),'synthetic sentinel; never executed')
[IO.File]::WriteAllText($oldConfig,'{"schemaVersion":1,"vrEnabled":false,"vrWorldScale":1.125}')
@{schemaVersion=1;converterVersion=1;layout='legacy';phase='completed';configPath=$oldConfig} |
    ConvertTo-Json | Set-Content -LiteralPath $oldReceipt -Encoding utf8
$sourceHash=(Get-FileHash -LiteralPath $oldConfig).Hash
$receiptHash=(Get-FileHash -LiteralPath $oldReceipt).Hash
function Assert([bool]$Value,[string]$Message) { if(-not $Value) { throw $Message } }
$plan=Join-Path $fixture 'plan.json'
& $ToolPath --plan $game $plan
Assert ($LASTEXITCODE -eq 0) 'Offline plan failed'
Assert (-not(Test-Path -LiteralPath (Join-Path $game 'gakumas-local'))) 'Planning changed the installation'
$planHash=(Get-FileHash -LiteralPath $plan).Hash.ToLowerInvariant()
$wrongHash='0'*64
& $ToolPath --apply $plan --expect-sha256 $wrongHash 2>&1 | Out-String | Write-Host
Assert ($LASTEXITCODE -ne 0) 'Wrong hash accepted'
Assert (-not(Test-Path -LiteralPath $active)) 'Wrong hash created active state'
& $ToolPath --apply $plan --expect-sha256 $planHash
Assert ($LASTEXITCODE -eq 0) 'Approved migration failed'
Assert ((Get-FileHash -LiteralPath $oldConfig).Hash -eq $sourceHash) 'Original config changed'
Assert ((Get-FileHash -LiteralPath $oldReceipt).Hash -eq $receiptHash) 'Old receipt changed'
Assert ((Get-FileHash -LiteralPath $newConfig).Hash -eq $sourceHash) 'Dedicated copy differs'
Assert (Test-Path -LiteralPath $active) 'Unique active state missing'
$activeHash=(Get-FileHash -LiteralPath $active).Hash
# A second real process must consume only the committed new layout.
[IO.File]::WriteAllText($oldConfig,'{obsolete invalid config')
[IO.File]::WriteAllText($oldReceipt,'{obsolete invalid receipt')
& $ToolPath --apply $plan --expect-sha256 $planHash
Assert ($LASTEXITCODE -eq 0) 'Repeated separate process consulted obsolete data'
Assert ((Get-FileHash -LiteralPath $active).Hash -eq $activeHash) 'Repeat rewrote active state'
$nextPlan=Join-Path $fixture 'after-restart.json'
& $ToolPath --plan $game $nextPlan
Assert ($LASTEXITCODE -eq 0) 'Restart preflight consulted old residue'
Assert ((Get-Content -LiteralPath $nextPlan -Raw | ConvertFrom-Json).action -eq 'already-managed') 'Restart source is not standard layout'
Write-Host "PASS: production offline utility plan/apply, hash gate and separate-process restart. Synthetic evidence: $fixture"
