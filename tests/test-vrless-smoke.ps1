#Requires -Version 7.0
[CmdletBinding()]
param(
    [switch]$RunMaintainerIntegration,
    [string]$PluginPath = '',
    [string]$GakuLoaderRoot = '',
    [string]$LocalifyRoot = '',
    [string]$RealOpenXrLoader = '',
    [ValidateRange(10, 120)][int]$ScenarioTimeoutSeconds = 35
)
$ErrorActionPreference = 'Stop'
if (-not $RunMaintainerIntegration) {
    Write-Host 'SKIP: optional production-host integration; pass -RunMaintainerIntegration and explicit external source roots.'
    return
}
if (-not $GakuLoaderRoot -or -not $LocalifyRoot) { throw 'Specify -GakuLoaderRoot and -LocalifyRoot for maintainer integration.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $PluginPath) { $PluginPath = Join-Path $projectRoot 'build/bin/plugin/x64/Release/GakuVRify.dll' }
if (-not $GakuLoaderRoot) { $GakuLoaderRoot = Join-Path (Split-Path -Parent $projectRoot) 'GakuLoader' }
$PluginPath = [IO.Path]::GetFullPath($PluginPath)
$GakuLoaderRoot = [IO.Path]::GetFullPath($GakuLoaderRoot)
if (-not $LocalifyRoot) { $LocalifyRoot = Join-Path $projectRoot '../../gkms-localify-dmm' }
$LocalifyRoot = [IO.Path]::GetFullPath($LocalifyRoot)
$cameraProviderHeader = Join-Path $LocalifyRoot 'src/host/CameraRenderProvider.hpp'
$cameraProviderAbi = Join-Path $LocalifyRoot 'src/host/GakuLocalifyCameraRender.h'
foreach ($path in @($cameraProviderHeader, $cameraProviderAbi)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Localify camera/render provider source missing: $path" }
}
$consumerAbi = Join-Path $projectRoot 'src/host/GakuLocalifyCameraRender.h'
if (-not (Test-Path -LiteralPath $consumerAbi)) { throw "VRify camera/render consumer ABI missing: $consumerAbi" }
# Git may check out the identical ABI as CRLF in one repository and LF in another.
# Decode strict UTF-8 and normalize only CRLF; every other character stays exact.
$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
$consumerAbiText = $strictUtf8.GetString([IO.File]::ReadAllBytes($consumerAbi)).Replace("`r`n", "`n")
$providerAbiText = $strictUtf8.GetString([IO.File]::ReadAllBytes($cameraProviderAbi)).Replace("`r`n", "`n")
if (-not [string]::Equals($consumerAbiText, $providerAbiText, [StringComparison]::Ordinal)) {
    throw 'Localify camera/render provider and VRify consumer ABI headers differ.'
}
if (-not (Test-Path -LiteralPath $PluginPath)) { throw "Build the production plugin first: $PluginPath" }
if (-not $RealOpenXrLoader) { $RealOpenXrLoader = Join-Path $projectRoot '.tools/openxr-loader-1.1.61/x64/bin/openxr_loader.dll' }
if (-not (Test-Path -LiteralPath $RealOpenXrLoader) -or
    (Get-FileHash -LiteralPath $RealOpenXrLoader).Hash -ne '0C262C9384BE6BC82634E03A324E2344CDFF41A4140FA8935891666184E8C596') {
    throw 'Pinned Khronos OpenXR loader missing or changed; run package-plugin.ps1 or pass -RealOpenXrLoader.'
}
$buildRoot = Join-Path $projectRoot 'build/vrless-smoke'
$compileRoot = Join-Path $buildRoot 'compiler'
New-Item -ItemType Directory -Force -Path $compileRoot | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -prerelease -products '*' -version '[17.0,)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vsRoot) { throw 'Visual Studio 2022+ C++ tools are required.' }
$developerCommand = Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
$fixtureRoot = Join-Path $PSScriptRoot 'vrless'
$hostSources = @('PluginHost.cpp', 'ActorEntry.cpp', 'Hotkeys.cpp', 'Trace.cpp', 'CrashTrace.cpp') |
    ForEach-Object { Join-Path $GakuLoaderRoot "src/$_" }
$minhookRoot = Join-Path $GakuLoaderRoot 'deps/minhook'
$minhookSources = @('buffer.c', 'hook.c', 'trampoline.c', 'hde/hde64.c') |
    ForEach-Object { Join-Path $minhookRoot "src/$_" }
foreach ($source in @($hostSources) + @($minhookSources) + @(
    (Join-Path $fixtureRoot 'VrlessSmoke.cpp'), (Join-Path $fixtureRoot 'FakeGameAssembly.cpp'),
    (Join-Path $fixtureRoot 'FakeOpenXrLoader.cpp'), (Join-Path $fixtureRoot 'FakeCameraProvider.cpp'))) {
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing smoke source: $source" }
}
function Compile-Command([string]$Command, [string]$Name) {
    $log = Join-Path $buildRoot "$Name-build.log"
    & $env:ComSpec /d /s /c ('call "{0}" -arch=x64 -host_arch=x64 && {1}' -f $developerCommand, $Command) *> $log
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $log -Tail 60; throw "$Name compilation failed: $log" }
}
$quotedMinhook = ($minhookSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
Compile-Command ('cl.exe /nologo /TC /c /MT /O2 /I"{0}/include" /Fo:"{1}\\" {2}' -f `
    $minhookRoot, $compileRoot, $quotedMinhook) 'minhook'
$exe = Join-Path $buildRoot 'vrless-smoke.exe'
$quotedHost = ($hostSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
Compile-Command ('cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /Od /Zi /DNOMINMAX /I"{0}/include" /I"{0}/src" /I"{1}/include" /I"{2}/src" /Fo:"{3}\\" /Fd:"{3}/vrless-host.pdb" /Fe:"{4}" "{5}/VrlessSmoke.cpp" {6} "{3}/buffer.obj" "{3}/hook.obj" "{3}/trampoline.obj" "{3}/hde64.obj" /link user32.lib advapi32.lib /INCREMENTAL:NO' -f `
    $GakuLoaderRoot, $minhookRoot, $projectRoot, $compileRoot, $exe, $fixtureRoot, $quotedHost) 'host-core'
$fakeAssembly = Join-Path $buildRoot 'GameAssembly.dll'
Compile-Command ('cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /Od /Ob0 /LD /Fo:"{0}/FakeGameAssembly.obj" /Fe:"{1}" "{2}/FakeGameAssembly.cpp" /link /OPT:NOICF /INCREMENTAL:NO' -f `
    $compileRoot, $fakeAssembly, $fixtureRoot) 'metadata-fixture'
$fakeBackend = Join-Path $buildRoot 'openxr_loader.dll'
Compile-Command ('cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /Od /LD /I"{0}/src" /Fo:"{1}/FakeOpenXrLoader.obj" /Fe:"{2}" "{3}/FakeOpenXrLoader.cpp" /link /INCREMENTAL:NO' -f `
    $projectRoot, $compileRoot, $fakeBackend, $fixtureRoot) 'backend-fixture'
$fakeProvider = Join-Path $buildRoot 'GakuLocalify.dll'
Compile-Command ('cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /Od /Ob0 /LD /I"{0}/src/host" /I"{1}/include" /Fo:"{2}/FakeCameraProvider.obj" /Fe:"{3}" "{4}/FakeCameraProvider.cpp" /link /OPT:NOICF /INCREMENTAL:NO' -f `
    $LocalifyRoot, $GakuLoaderRoot, $compileRoot, $fakeProvider, $fixtureRoot) 'camera-provider-fixture'

$runRoot = Join-Path $buildRoot ('run-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
$scenarios = @('native-loader-missing', 'native-runtime-missing', 'native-device-missing',
    'unity-loader-missing', 'unity-runtime-missing', 'unity-device-missing',
    'native-real-runtime-missing', 'unity-real-runtime-missing',
    'native-invalid-loader', 'unity-metadata-missing',
    'coexist-runtime-missing', 'coexist-device-missing', 'coexist-target-mismatch')
$results = @()
foreach ($scenario in $scenarios) {
    $game = Join-Path $runRoot $scenario
    $plugins = Join-Path $game 'gakumas-local/plugins'
    $vr = Join-Path $game 'gakumas-vr'
    New-Item -ItemType Directory -Force -Path $plugins, $vr | Out-Null
    Copy-Item -LiteralPath $PluginPath -Destination (Join-Path $plugins 'GakuVRify.dll')
    if ($scenario.StartsWith('coexist-')) { Copy-Item -LiteralPath $fakeProvider -Destination (Join-Path $plugins 'GakuLocalify.dll') }
    [ordered]@{ vrEnabled = $true; vrNativeOnly = $scenario.StartsWith('native-');
        vrHeadPoseEnabled = $true; vrStereoEnabled = $true; vrDiagnosticsEnabled = $scenario -ne 'native-loader-missing';
        vrLocalizeText = $false; vrLiveGaze = $false } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $vr 'config.json') -Encoding utf8
    '[diagnostics]', 'trace=true' | Set-Content -LiteralPath (Join-Path $game 'gakuloader.ini') -Encoding utf8
    if ($scenario.StartsWith('unity-') -or $scenario.StartsWith('coexist-')) { Copy-Item -LiteralPath $fakeAssembly -Destination (Join-Path $game 'GameAssembly.dll') }
    if ($scenario.EndsWith('invalid-loader')) {
        Set-Content -LiteralPath (Join-Path $game 'openxr_loader.dll') -Value 'invalid OpenXR DLL negative control'
    } elseif ($scenario.Contains('real-runtime-missing')) {
        Copy-Item -LiteralPath $RealOpenXrLoader -Destination (Join-Path $game 'openxr_loader.dll')
    } elseif ($scenario.EndsWith('runtime-missing') -or $scenario.EndsWith('device-missing')) {
        Copy-Item -LiteralPath $fakeBackend -Destination (Join-Path $game 'openxr_loader.dll')
    }
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $exe
    $start.ArgumentList.Add($scenario)
    $start.ArgumentList.Add($game)
    $start.WorkingDirectory = $game
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    $clock = [Diagnostics.Stopwatch]::StartNew()
    if (-not $process.Start()) { throw "Could not start scenario: $scenario" }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($ScenarioTimeoutSeconds * 1000)
    if ($timedOut) { $process.Kill($true); $process.WaitForExit() }
    $clock.Stop()
    [IO.File]::WriteAllText((Join-Path $game 'stdout.log'), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $game 'stderr.log'), $stderr.GetAwaiter().GetResult())
    $resultFile = Join-Path $game 'result.json'
    $child = if (Test-Path -LiteralPath $resultFile) { Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json -AsHashtable } else { $null }
    $result = [ordered]@{ scenario = $scenario; exitCode = $process.ExitCode; timedOut = $timedOut;
        elapsedMs = $clock.ElapsedMilliseconds; evidenceDirectory = $game; child = $child;
        status = if (-not $timedOut -and $process.ExitCode -eq 0 -and $child.status -eq 'PASS') { 'PASS' } else { 'FAIL' } }
    $results += $result
    Write-Host "$($result.status): $scenario (exit=$($process.ExitCode), timeout=$timedOut, $($clock.ElapsedMilliseconds)ms)"
    if ($result.status -ne 'PASS') {
        Get-Content -LiteralPath (Join-Path $game 'stdout.log') -Tail 15
        Get-Content -LiteralPath (Join-Path $game 'stderr.log') -Tail 15
    }
}
$hostHashes = [ordered]@{}
foreach ($source in @($hostSources) + @($minhookSources)) {
    $hostHashes[[IO.Path]::GetRelativePath($GakuLoaderRoot, $source).Replace('\','/')] = (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant()
}
$report = [ordered]@{ test = 'GakuVRify VR-less smoke'; created = (Get-Date -Format o);
    status = if (@($results | Where-Object { $_.status -ne 'PASS' }).Count) { 'FAIL' } else { 'PASS' };
    plugin = $PluginPath; pluginSha256 = (Get-FileHash -LiteralPath $PluginPath).Hash.ToLowerInvariant();
    host = 'production GakuLoader core without test-seam macro; desktop UI/dialog shell stubbed';
    hostSourceHashes = $hostHashes; metadataFixtureSha256 = (Get-FileHash -LiteralPath $fakeAssembly).Hash.ToLowerInvariant();
    backendFixtureSha256 = (Get-FileHash -LiteralPath $fakeBackend).Hash.ToLowerInvariant();
    realOpenXrLoaderSha256 = (Get-FileHash -LiteralPath $RealOpenXrLoader).Hash.ToLowerInvariant();
    cameraProviderFixtureSha256 = (Get-FileHash -LiteralPath $fakeProvider).Hash.ToLowerInvariant();
    cameraProviderSourceSha256 = (Get-FileHash -LiteralPath $cameraProviderHeader).Hash.ToLowerInvariant();
    cameraProviderAbiSha256 = (Get-FileHash -LiteralPath $cameraProviderAbi).Hash.ToLowerInvariant();
    cameraConsumerAbiSha256 = (Get-FileHash -LiteralPath $consumerAbi).Hash.ToLowerInvariant();
    limits = @('IL2CPP metadata is a fixture, not real Unity/game execution',
        'OpenXR test backend never supplies a system/session/pose',
        'The host window/frame-pump bootstrap and headset visuals are not tested',
        'Coexist scenarios use the production CameraRenderProvider class in a fixture DLL, not the full translation plugin'); scenarios = $results }
$report | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $runRoot 'report.json') -Encoding utf8
Write-Host "VR-less smoke evidence: $runRoot/report.json"
if ($report.status -ne 'PASS') { throw 'VR-less smoke has failed scenarios; inspect the recorded evidence.' }
