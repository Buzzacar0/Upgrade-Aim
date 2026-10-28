<#
.SYNOPSIS
  Configures, builds and tests StickyAim Lab from a plain PowerShell prompt.

.DESCRIPTION
  Locates Visual Studio (Build Tools are enough) with vswhere, imports the x64
  developer environment, then runs CMake with the selected preset.

.EXAMPLE
  .\scripts\build.ps1                     # release preset, build + test
  .\scripts\build.ps1 -Preset core        # portable core + tests only
  .\scripts\build.ps1 -QtRoot D:\Qt\6.8.3\msvc2022_64 -NoTest
#>
param(
    [ValidateSet('release', 'debug', 'core')]
    [string]$Preset = 'release',
    [string]$QtRoot = $env:QT_ROOT,
    [switch]$NoTest,
    [switch]$Deploy
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found. Install Visual Studio 2022 or Build Tools 2022 with the C++ workload.' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'No Visual Studio installation with the MSVC x64 toolset was found.' }

# Import the developer environment (cl, link, Windows SDK, bundled cmake/ninja).
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$env:PATH = "$(Split-Path $vswhere);$env:PATH"  # vcvars itself calls vswhere
cmd /c "`"$vcvars`" >nul 2>nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2]) }
}

if ($Preset -ne 'core') {
    if (-not $QtRoot) {
        $guess = Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^6\.' } | Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName 'msvc2022_64' } | Where-Object { Test-Path $_ } | Select-Object -First 1
        $QtRoot = $guess
    }
    if (-not $QtRoot -or -not (Test-Path $QtRoot)) { throw 'Qt 6 (msvc2022_64) not found. Pass -QtRoot or set QT_ROOT.' }
    $env:QT_ROOT = $QtRoot
    Write-Host "Using Qt at $QtRoot"
}

# Native tools report failure through exit codes; their stderr output (e.g.
# CMake warnings) must not abort the script under Windows PowerShell 5.1.
$ErrorActionPreference = 'Continue'
Push-Location $repo
try {
    cmake --preset $Preset
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    cmake --build --preset $Preset
    if ($LASTEXITCODE) { throw 'Build failed' }
    if (-not $NoTest) {
        ctest --preset $(if ($Preset -eq 'core') { 'core' } else { 'release' }) -j 4
        if ($LASTEXITCODE) { throw 'Tests failed' }
    }
    if ($Deploy -and $Preset -ne 'core') {
        $exe = Join-Path $repo "build\$Preset\src\app\StickyAimLab.exe"
        & (Join-Path $QtRoot 'bin\windeployqt.exe') --no-translations --no-system-d3d-compiler $exe
    }
}
finally { Pop-Location }
