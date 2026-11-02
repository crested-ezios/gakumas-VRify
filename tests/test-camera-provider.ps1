#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$CompilerVersion = '',
    [ValidateRange(1, 120)]
    [int]$TimeoutSeconds = 20
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $projectRoot 'scripts/select-msvc.ps1')
$toolchain = Get-VrifyToolchain -CompilerVersion $CompilerVersion
if ($toolchain.ToolsVersion -notmatch '^\d+\.\d+(?:\.\d+){0,2}$') {
    throw 'Toolchain returned an invalid MSVC tools version.'
}

$buildRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build/camera-provider-tests'))
$buildPrefix = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build')) + [IO.Path]::DirectorySeparatorChar
if (-not $buildRoot.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase) -or
    (Split-Path -Leaf $buildRoot) -ne 'camera-provider-tests') {
    throw "Unexpected camera-provider test output directory: $buildRoot"
}
$source = Join-Path $projectRoot 'tests/cpp/camera_begin_provider_tests.cpp'
$sourceInclude = Join-Path $projectRoot 'src'
$sdkInclude = Join-Path $projectRoot 'sdk/gakuloader'
foreach ($inputFile in @($source, (Join-Path $sdkInclude 'GakuPlugin.h'), $toolchain.DeveloperCommand)) {
    if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) {
        throw "Missing camera-provider test input: $inputFile"
    }
}
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$executable = Join-Path $buildRoot 'camera_begin_provider_tests.exe'
$object = Join-Path $buildRoot 'camera_begin_provider_tests.obj'
$compileCommand = 'cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /W4 /I"{0}" /I"{1}" /Fo:"{2}" /Fe:"{3}" "{4}"' -f `
    $sourceInclude, $sdkInclude, $object, $executable, $source
$driver = @(
    '@echo off',
    'set VSCMD_SKIP_SENDTELEMETRY=1',
    ('call "{0}" -arch=x64 -host_arch=x64 -vcvars_ver={1}' -f $toolchain.DeveloperCommand, $toolchain.ToolsVersion),
    'if errorlevel 1 exit /b %errorlevel%',
    'where cl.exe',
    $compileCommand,
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines((Join-Path $buildRoot 'compile.cmd'), $driver, [Text.UTF8Encoding]::new($false))
Write-Host ("Camera-provider tests: MSVC {0}; public provider and SDK only" -f $toolchain.ToolsVersion)
Push-Location $buildRoot
try {
    & $env:ComSpec /d /c compile.cmd 2>&1 | Tee-Object -FilePath (Join-Path $buildRoot 'build.log')
    $compileExit = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($compileExit -ne 0 -or -not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Camera-provider test compilation failed (exit $compileExit)."
}

# A regression that dispatches while holding the mutex would deadlock the reentry
# cases. Bound only this synthetic child process; never start a game or XR loader.
$start = [Diagnostics.ProcessStartInfo]::new($executable)
$start.WorkingDirectory = $buildRoot
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
try {
    if (-not $process.Start()) { throw 'Could not start camera-provider tests.' }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Camera-provider tests timed out after $TimeoutSeconds seconds (possible reentry deadlock)."
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $buildRoot 'run.log'), $stdout + $stderr, [Text.UTF8Encoding]::new($false))
    if ($stdout) { Write-Host $stdout.TrimEnd() }
    if ($stderr) { Write-Host $stderr.TrimEnd() }
    if ($process.ExitCode -ne 0) { throw "Camera-provider tests exited with code $($process.ExitCode)." }
} finally {
    $process.Dispose()
}
