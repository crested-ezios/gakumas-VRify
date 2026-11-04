#Requires -Version 7.0
[CmdletBinding()]
param([string]$StandalonePath = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $StandalonePath) { $StandalonePath = Join-Path $projectRoot 'build/bin/x64/Release/version.dll' }
$StandalonePath = [IO.Path]::GetFullPath($StandalonePath)
if (-not (Test-Path -LiteralPath $StandalonePath -PathType Leaf)) {
    throw "Build the production standalone first: ./build.ps1 -Mode Standalone -Configuration Release ($StandalonePath)"
}
& python (Join-Path $projectRoot 'scripts/source-pipeline.py') verify-binary --binary $StandalonePath --mode Standalone
if ($LASTEXITCODE -ne 0) { throw 'Refusing to smoke-test a stale or unverified standalone build.' }

# No installation path is accepted. Every executable, DLL and config belongs to
# this fresh synthetic fixture under the repository's ignored build directory.
$runRoot = Join-Path $projectRoot ('build/standalone-smoke/run-' + [guid]::NewGuid().ToString('N'))
$game = Join-Path $runRoot 'game fixture'
$compiler = Join-Path $runRoot 'compiler'
$workingDirectory = Join-Path $runRoot 'unrelated-cwd'
$local = Join-Path $game 'gakumas-local'
$vr = Join-Path $game 'gakumas-vr'
New-Item -ItemType Directory -Path $compiler, $workingDirectory, $local, $vr | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $game '.standalone-smoke-fixture'), "SYNTHETIC-ONLY`n", $utf8)
$configurations = [ordered]@{
    'gakumas-local/config.json' = '{"enableConsole":false,"useRemoteAssets":false,"useAPIAssets":false,"useAPITextureAssets":false,"smokeSentinel":"program-original"}'
    'gakumas-local/localizationConfig.json' = '{"enabled":false,"lazyInit":false,"replaceFont":false,"replaceTexture":false,"useMasterTrans":false,"smokeSentinel":"localify-original"}'
    'gakumas-vr/config.json' = '{"schemaVersion":1,"vrEnabled":true,"vrNativeOnly":true,"vrDiagnosticsEnabled":true,"vrHeadPoseEnabled":false,"vrStereoEnabled":false,"vrLocalizeText":false,"smokeSentinel":"vr-original"}'
}
$before = [ordered]@{}
foreach ($relative in $configurations.Keys) {
    $path = Join-Path $game $relative
    [IO.File]::WriteAllText($path, $configurations[$relative] + "`n", $utf8)
    $before[$relative] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}
Copy-Item -LiteralPath $StandalonePath -Destination (Join-Path $game 'version.dll')

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -prerelease -products '*' -version '[17.0,)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'Visual Studio 2022+ C++ tools are required.' }
$developerCommand = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$source = Join-Path $PSScriptRoot 'vrless/StandaloneSmoke.cpp'
$exe = Join-Path $game 'gakumas.exe'
function Compile-Fixture([string]$Command, [string]$Name) {
    $log = Join-Path $runRoot "$Name-build.log"
    & $env:ComSpec /d /s /c ('call "{0}" -arch=x64 -host_arch=x64 && {1}' -f $developerCommand, $Command) *> $log
    if ($LASTEXITCODE -ne 0) {
        Get-Content -LiteralPath $log -Tail 30
        throw "Standalone smoke fixture compilation failed: $log"
    }
}
Compile-Fixture ('cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /O2 /Fo:"{0}/StandaloneSmoke.obj" /Fe:"{1}" "{2}" /link /INCREMENTAL:NO' -f $compiler, $exe, $source) 'runner'

$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $exe
$start.WorkingDirectory = $workingDirectory
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.StandardOutputEncoding = $utf8
$start.StandardErrorEncoding = $utf8
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
$clock = [Diagnostics.Stopwatch]::StartNew()
try {
    if (-not $process.Start()) { throw "Could not start the synthetic standalone fixture: $exe" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit(20000)
    if ($timedOut) {
        # This handle was just created above; never locate/kill real game processes.
        $process.Kill()
        if (-not $process.WaitForExit(5000)) { throw "Timed-out synthetic process did not exit after kill: $($process.Id)" }
    }
    $exitCode = $process.ExitCode
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
} finally {
    $clock.Stop()
    $process.Dispose()
}
[IO.File]::WriteAllText((Join-Path $runRoot 'stdout.log'), $stdout, $utf8)
[IO.File]::WriteAllText((Join-Path $runRoot 'stderr.log'), $stderr, $utf8)
$after = [ordered]@{}
$configUnchanged = $true
foreach ($relative in $before.Keys) {
    $path = Join-Path $game $relative
    $after[$relative] = if (Test-Path -LiteralPath $path -PathType Leaf) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { $null }
    if ($after[$relative] -ne $before[$relative]) { $configUnchanged = $false }
}
$passed = -not $timedOut -and $exitCode -eq 0 -and $configUnchanged -and $stdout.Contains('STANDALONE_PASS ')
$report = [ordered]@{
    test = 'GakuVRify standalone native missing-loader smoke'
    status = if ($passed) { 'PASS' } else { 'FAIL' }
    standalone = $StandalonePath
    standaloneSha256 = (Get-FileHash -LiteralPath $StandalonePath -Algorithm SHA256).Hash.ToLowerInvariant()
    fixture = $game
    workingDirectory = $workingDirectory
    exitCode = $exitCode
    timedOut = $timedOut
    elapsedMs = $clock.ElapsedMilliseconds
    configUnchanged = $configUnchanged
    configurationBefore = $before
    configurationAfter = $after
    stdout = $stdout.Trim()
    stderr = $stderr.Trim()
    scope = 'Production Standalone DllMain/bootstrap/proxy and native-only stop; no real game, Unity metadata or OpenXR loader.'
}
$reportPath = Join-Path $runRoot 'report.json'
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Host "$($report.status): Standalone bootstrap, missing XR loader, stop, proxy, plugin isolation, config and CWD ($($clock.ElapsedMilliseconds)ms)"
Write-Host "Standalone smoke evidence: $reportPath"
if (-not $passed) {
    Write-Host $stdout
    Write-Host $stderr
    throw 'Standalone smoke failed; inspect the recorded synthetic fixture.'
}
