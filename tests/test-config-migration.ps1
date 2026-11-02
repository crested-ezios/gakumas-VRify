#Requires -Version 7.0
[CmdletBinding()]
param([string]$JsonIncludeDirectory='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(-not $JsonIncludeDirectory) { $JsonIncludeDirectory=(Get-Content -LiteralPath (Join-Path $projectRoot 'build/vs2022/Plugin/Release/nlohmann-json-include.txt') -Raw).Trim() }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot=& $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if($LASTEXITCODE -ne 0 -or -not $vsRoot) { throw 'MSVC tools missing.' }
$developerCommand=Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
$buildRoot=Join-Path $projectRoot 'build/config-migration-tests'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$source=Join-Path $PSScriptRoot 'cpp/config_migration_tests.cpp'
$implementation=Join-Path $projectRoot 'src/vr/config/VrConfigStartup.cpp'
$executable=Join-Path $buildRoot 'config-migration-tests.exe'
$command='call "{0}" -arch=x64 -host_arch=x64 && cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /I"{1}" /I"{2}/src" /Fo:"{3}\\" /Fe:"{4}" "{5}" "{6}" bcrypt.lib' -f $developerCommand,$JsonIncludeDirectory,$projectRoot,$buildRoot,$executable,$source,$implementation
& $env:ComSpec /d /s /c $command
if($LASTEXITCODE -ne 0) { throw 'Configuration migration test compilation failed.' }
$runRoot=Join-Path $buildRoot ('run-中文 space-'+[Guid]::NewGuid().ToString('N'))
& $executable $runRoot
if($LASTEXITCODE -ne 0) { throw "Configuration migration behavior failed: $runRoot" }
Write-Host "PASS: synthetic migration and restart tests. Evidence: $runRoot"
