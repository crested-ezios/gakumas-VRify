#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$PluginPath = '',
    [string]$LoaderSdkDirectory = '',
    [string]$CompilerVersion = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $PluginPath) {
    $PluginPath = Join-Path $projectRoot "build/bin/plugin/x64/$Configuration/GakuVRify.dll"
}
if (-not $LoaderSdkDirectory) {
    $LoaderSdkDirectory = Join-Path $projectRoot 'sdk/gakuloader'
}
if (-not (Test-Path -LiteralPath $PluginPath)) { throw "Build the production plugin first: $PluginPath" }
if (-not (Test-Path -LiteralPath (Join-Path $LoaderSdkDirectory 'GakuPlugin.h'))) {
    throw "GakuLoader SDK missing: $LoaderSdkDirectory"
}
. (Join-Path $projectRoot 'scripts/select-msvc.ps1')
$toolchain = Get-VrifyToolchain -CompilerVersion $CompilerVersion
$developerCommand = $toolchain.DeveloperCommand
$buildRoot = Join-Path $projectRoot 'build/hosted-plugin-tests'
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$source = Join-Path $PSScriptRoot 'cpp/hosted_plugin_tests.cpp'
$executable = Join-Path $buildRoot 'hosted-plugin-tests.exe'
$command = 'call "{0}" -arch=x64 -host_arch=x64 -vcvars_ver={7} && cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /I"{1}" /Fo:"{2}\\" /Fe:"{3}" "{4}" user32.lib' -f `
    $developerCommand, $LoaderSdkDirectory, $buildRoot, $executable, $source, $null, $null, $toolchain.ToolsVersion
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'Hosted production-plugin test compilation failed.' }
$hookSource = Join-Path $PSScriptRoot 'cpp/hosted_hooks_tests.cpp'
$hookImplementation = Join-Path $projectRoot 'src/host/HostedHooks.cpp'
$hookExecutable = Join-Path $buildRoot 'hosted-hooks-tests.exe'
$hookCommand = 'call "{0}" -arch=x64 -host_arch=x64 -vcvars_ver={7} && cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /DGAKU_VRIFY_HOSTED=1 /I"{1}" /I"{2}/src" /Fo:"{3}\\" /Fe:"{4}" "{5}" "{6}"' -f `
    $developerCommand, $LoaderSdkDirectory, $projectRoot, $buildRoot, $hookExecutable, $hookSource, $hookImplementation, $toolchain.ToolsVersion
& $env:ComSpec /d /s /c $hookCommand
if ($LASTEXITCODE -ne 0) { throw 'Hosted hook-facade test compilation failed.' }
& $hookExecutable
if ($LASTEXITCODE -ne 0) { throw 'Hosted hook-facade behavior failed.' }
$runRoot = Join-Path $buildRoot ('run-' + [Guid]::NewGuid().ToString('N'))
$scenarios = @('abi', 'missing:gakuloader.log', 'missing:gakuloader.runtime',
    'missing:gakuloader.hooks', 'missing:gakuloader.actor-entry', 'service-conflict',
    'disabled', 'enabled-without-runtime', 'invalid-vr-config')
foreach ($scenario in $scenarios) {
    $scenarioRoot = Join-Path $runRoot ($scenario.Replace(':', '-'))
    & $executable $PluginPath $scenario $scenarioRoot
    if ($LASTEXITCODE -ne 0) { throw "Hosted production-plugin test failed: $scenario" }
}
Write-Host "PASS: $($scenarios.Count) isolated production DLL lifecycle/contract scenarios"
Write-Host "Test configuration evidence: $runRoot"
