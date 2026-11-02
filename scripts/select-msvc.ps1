#Requires -Version 7.0
# Shared by builds and isolated native tests; changes only this process's tools.
function Get-VrifyToolchain {
    param([string]$CompilerVersion = '')
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer vswhere.exe is required.' }
    $instances = & $vswhere -latest -prerelease -products '*' -version '[17.0,)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or -not $instances) { throw 'Visual Studio 2022+ Desktop C++ tools are required.' }
    $instance = @($instances)[0]
    $toolsRoot = Join-Path $instance.installationPath 'VC/Tools/MSVC'
    $available = @(Get-ChildItem -LiteralPath $toolsRoot -Directory | Where-Object {
        $_.Name -match '^14\.\d+\.\d+$' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'bin/Hostx64/x64/cl.exe'))
    } | Sort-Object { [version]$_.Name } -Descending)
    $tool = if ($CompilerVersion) { $available | Where-Object Name -EQ $CompilerVersion | Select-Object -First 1 } else { $available | Select-Object -First 1 }
    if (-not $tool) { throw "MSVC x64 toolset unavailable: $CompilerVersion" }
    if ([version]$tool.Name -lt [version]'14.40') { throw 'This dependency ABI requires MSVC 14.40 or newer.' }
    [pscustomobject]@{
        Root = $instance.installationPath
        Generator = if ([version]$instance.installationVersion -ge [version]'18.0') { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
        ToolsVersion = $tool.Name
        ConanCompiler = (190 + [math]::Floor(([version]$tool.Name).Minor / 10)).ToString()
        DeveloperCommand = Join-Path $instance.installationPath 'Common7/Tools/VsDevCmd.bat'
    }
}
