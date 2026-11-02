#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Plugin', 'Standalone')]
    [string]$Mode = 'Plugin',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 32)]
    [int]$CompilerProcesses = 4,
    [string]$LoaderSdkDirectory = '',
    [string]$CompilerVersion = '',
    [ValidateSet('Auto', '194', '195')]
    [string]$DependencyCompilerVersion = 'Auto',
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
if ($Offline -and -not (Test-Path -LiteralPath (Join-Path $projectRoot '.upstream/localify/.git'))) { throw 'Offline build requires the locked upstream checkout already present.' }
& (Join-Path $projectRoot 'scripts/fetch-upstream.ps1')
$venvRoot = Join-Path $projectRoot '.tools/build'
$pythonExe = Join-Path $venvRoot 'Scripts/python.exe'
$conanExe = Join-Path $venvRoot 'Scripts/conan.exe'
$cmakeExe = Join-Path $venvRoot 'Scripts/cmake.exe'
$buildRoot = if ($Mode -eq 'Plugin') {
    Join-Path $projectRoot "build/vs2022/Plugin/$Configuration"
} else {
    Join-Path $projectRoot "build/vs2022/$Configuration"
}
if ([string]::IsNullOrWhiteSpace($LoaderSdkDirectory)) {
    $LoaderSdkDirectory = Join-Path $projectRoot 'sdk/gakuloader'
}
if ($Mode -eq 'Plugin' -and -not (Test-Path -LiteralPath (Join-Path $LoaderSdkDirectory 'GakuPlugin.h'))) {
    throw "GakuLoader SDK header missing: $LoaderSdkDirectory/GakuPlugin.h"
}
$lockfile = Join-Path $projectRoot 'conan-release.lock'
$profile = Join-Path $buildRoot 'dependency.profile'
. (Join-Path $projectRoot 'scripts/select-msvc.ps1')
$toolchain = Get-VrifyToolchain -CompilerVersion $CompilerVersion
$vsRoot = $toolchain.Root
$cmakeGenerator = $toolchain.Generator
$dependencyCompiler = if ($DependencyCompilerVersion -eq 'Auto') { $toolchain.ConanCompiler } else { $DependencyCompilerVersion }
if ([int]$dependencyCompiler -gt [int]$toolchain.ConanCompiler) { throw 'The consumer toolset must be at least as new as the selected dependency ABI.' }
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
@"
[settings]
os=Windows
arch=x86_64
compiler=msvc
compiler.version=$dependencyCompiler
compiler.cppstd=23
compiler.runtime=dynamic
build_type=$Configuration
[conf]
tools.cmake.cmaketoolchain:generator=$cmakeGenerator
"@ | Set-Content -LiteralPath $profile -Encoding utf8NoBOM
Write-Host "MSVC consumer toolset: $($toolchain.ToolsVersion); dependency ABI: msvc$dependencyCompiler"
if (-not (Test-Path $pythonExe)) {
    & python -m venv $venvRoot
    if ($LASTEXITCODE -ne 0) { throw 'Failed to create the build environment.' }
}
[string[]]$pipOptions = @()
if ($Offline) { $pipOptions = @('--no-index') }
& $pythonExe -m pip install @pipOptions --disable-pip-version-check -r (Join-Path $projectRoot 'scripts/build-requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Failed to install build tools.' }
# Use the official wheel's native executable directly on Windows.
# This retains its adjacent module tree and avoids the Python console wrapper.
$nativeCmake = Join-Path $venvRoot 'Lib/site-packages/cmake/data/bin/cmake.exe'
if (Test-Path -LiteralPath $nativeCmake) { $cmakeExe = $nativeCmake }
$previousConanHome = $env:CONAN_HOME
$previousPath = $env:PATH
try {
    $env:CONAN_HOME = Join-Path $projectRoot '.tools/conan2'
    $env:PATH = (Split-Path -Parent $cmakeExe) + [IO.Path]::PathSeparator + (Join-Path $venvRoot 'Scripts') + [IO.Path]::PathSeparator + $env:PATH
    # Windows pip launchers embed the venv's absolute Python path. A copied
    # inventory checkout can keep usable Python packages but unusable launchers.
    # Keep the check inside our Conan home; version probing must not touch the
    # user's global Conan cache when this project pins a different CLI version.
    & $conanExe --version | Out-Null
    $conanLauncherReady = $LASTEXITCODE -eq 0
    & $cmakeExe --version | Out-Null
    if (-not $conanLauncherReady -or $LASTEXITCODE -ne 0) {
        & $pythonExe -m pip install @pipOptions --disable-pip-version-check --force-reinstall `
            -r (Join-Path $projectRoot 'scripts/build-requirements.txt')
        if ($LASTEXITCODE -ne 0) { throw 'Failed to repair relocated build-tool launchers.' }
    }
    & (Join-Path $projectRoot 'scripts/prepare-source.ps1')
    [string[]]$conanOptions = @('--build=missing')
    if ($Offline) { $conanOptions = @('--no-remote', '--build=never') }
    & $conanExe install (Join-Path $projectRoot 'scripts/conanfile.py') `
        --output-folder $buildRoot --lockfile $lockfile @conanOptions `
        -pr:h $profile -pr:b $profile -s "build_type=$Configuration" `
        -c "tools.build:jobs=$CompilerProcesses" `
        -c "tools.cmake.cmaketoolchain:generator=$cmakeGenerator"
    if ($LASTEXITCODE -ne 0) { throw 'Conan dependency installation failed.' }
    & $cmakeExe -S (Join-Path $projectRoot 'scripts') -B $buildRoot `
        -G $cmakeGenerator -A x64 -T "version=$($toolchain.ToolsVersion)" `
        "-DCMAKE_GENERATOR_INSTANCE=$vsRoot" `
        "-DCMAKE_CONFIGURATION_TYPES=$Configuration" `
        "-DVR_BUILD_MODE=$Mode" `
        "-DGAKULOADER_SDK_DIR=$LoaderSdkDirectory" `
        "-DVR_COMPILER_PROCESSES=$CompilerProcesses" `
        "-DCMAKE_TOOLCHAIN_FILE=$buildRoot/conan_toolchain.cmake"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmakeExe --build $buildRoot --config $Configuration --parallel $CompilerProcesses
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
} finally {
    $env:CONAN_HOME = $previousConanHome
    $env:PATH = $previousPath
}
$outputPath = if ($Mode -eq 'Plugin') {
    Join-Path $projectRoot "build/bin/plugin/x64/$Configuration/GakuVRify.dll"
} else {
    Join-Path $projectRoot "build/bin/x64/$Configuration/version.dll"
}
if (-not (Test-Path $outputPath)) { throw "Missing build output: $outputPath" }
Write-Host "Built: $outputPath"
