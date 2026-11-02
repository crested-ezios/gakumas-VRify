#Requires -Version 7.0
[CmdletBinding()]
param([string]$JsonIncludeDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $JsonIncludeDirectory) {
    $includeReceipt = Join-Path $projectRoot 'build/vs2022/Plugin/Release/nlohmann-json-include.txt'
    if (-not (Test-Path -LiteralPath $includeReceipt -PathType Leaf)) {
        throw 'Build the plugin first, or pass -JsonIncludeDirectory containing nlohmann/json.hpp.'
    }
    $JsonIncludeDirectory = (Get-Content -LiteralPath $includeReceipt -Raw).Trim()
}
if (-not (Test-Path -LiteralPath (Join-Path $JsonIncludeDirectory 'nlohmann/json.hpp') -PathType Leaf)) {
    throw "nlohmann/json.hpp is missing: $JsonIncludeDirectory"
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer vswhere.exe is required.' }
$vsRoot = & $vswhere -latest -prerelease -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vsRoot) { throw 'Visual Studio 2022+ Desktop C++ tools are required.' }
$developerCommand = Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
$buildRoot = Join-Path $projectRoot 'build/config-startup-tests'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$source = Join-Path $PSScriptRoot 'cpp/config_startup_tests.cpp'
$implementation = Join-Path $projectRoot 'src/vr/config/VrConfigStartup.cpp'
$executable = Join-Path $buildRoot 'config-startup-tests.exe'
$command = 'call "{0}" -arch=x64 -host_arch=x64 && cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /I"{1}" /I"{2}/src" /Fo:"{3}\\" /Fe:"{4}" "{5}" "{6}" bcrypt.lib' -f `
    $developerCommand, $JsonIncludeDirectory, $projectRoot, $buildRoot, $executable, $source, $implementation
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'Production configuration-startup test compilation failed.' }
# This tree contains only generated small JSON fixtures, never a real install.
# Keep it as evidence; each invocation requires an absent unique run directory.
$runRoot = Join-Path $buildRoot ('run-中文 space-' + [Guid]::NewGuid().ToString('N'))
& $executable $runRoot
if ($LASTEXITCODE -ne 0) { throw "Configuration-startup behavior failed. Synthetic evidence: $runRoot" }
Write-Host "PASS: production configuration-startup tests. Synthetic evidence: $runRoot"
