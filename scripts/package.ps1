<#
.SYNOPSIS
  Builds the Upgrade Aim installer: dist\UpgradeAim-Setup-<version>.exe

.DESCRIPTION
  1. Release build in build\package (tests included).
  2. Stage: cmake --install + windeployqt into dist\stage (the CLI and the
     keygen are left out: the CLI could drive the virtual pad without an
     activation code, and the keygen holds your private key).
  3. Downloads the pinned ViGEmBus / HidHide installers (SHA-256 checked) and
     the third-party license texts.
  4. Compiles installer\UpgradeAim.iss with Inno Setup.
#>
param([switch]$NoTest)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $repo 'dist'
$stage = Join-Path $dist 'stage'
$redist = Join-Path $dist 'redist'

$buildArgs = @{ Preset = 'package' }
if ($NoTest) { $buildArgs.NoTest = $true }
& (Join-Path $PSScriptRoot 'build.ps1') @buildArgs
if ($LASTEXITCODE) { throw 'build failed' }

# The build script imported the MSVC environment into this process.
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage, $redist | Out-Null
$ErrorActionPreference = 'Continue'
cmake --install (Join-Path $repo 'build\package') --prefix $stage
if ($LASTEXITCODE) { throw 'cmake --install failed' }
$ErrorActionPreference = 'Stop'
Remove-Item -Force (Join-Path $stage 'bin\upgradeaim-cli.exe') -ErrorAction SilentlyContinue

$qt = $env:QT_ROOT
if (-not $qt) { $qt = Get-ChildItem 'C:\Qt' -Directory | Where-Object Name -match '^6\.' | Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'msvc2022_64' } | Select-Object -First 1 }
$ErrorActionPreference = 'Continue'
& (Join-Path $qt 'bin\windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $stage 'bin\UpgradeAim.exe')
if ($LASTEXITCODE) { throw 'windeployqt failed' }
$ErrorActionPreference = 'Stop'
$vc = Join-Path $stage 'bin\vc_redist.x64.exe'
if (Test-Path $vc) { Move-Item -Force $vc (Join-Path $redist 'vc_redist.x64.exe') }
if (-not (Test-Path (Join-Path $redist 'vc_redist.x64.exe'))) {
    Invoke-WebRequest -UseBasicParsing 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile (Join-Path $redist 'vc_redist.x64.exe')
}

# Pinned third-party installers, verified by hash and Authenticode signature.
$drivers = @(
    @{ Url = 'https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0_x64_x86_arm64.exe'
       Sha = '89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A' },
    @{ Url = 'https://github.com/nefarius/HidHide/releases/download/v1.5.230.0/HidHide_1.5.230_x64.exe'
       Sha = 'F4BBBCB82E6258641B887C74BC81C4C5F66E4AA811808DFC304347687B7605F6' }
)
foreach ($d in $drivers) {
    $file = Join-Path $redist ([IO.Path]::GetFileName($d.Url))
    if (-not (Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash -ne $d.Sha) {
        Invoke-WebRequest -UseBasicParsing $d.Url -OutFile $file
    }
    if ((Get-FileHash $file -Algorithm SHA256).Hash -ne $d.Sha) { throw "hash mismatch: $file" }
    if ((Get-AuthenticodeSignature $file).Status -ne 'Valid') { throw "invalid signature: $file" }
}

# Third-party licenses shipped with the app.
$lic = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Force $lic | Out-Null
$texts = @{
    'ViGEmBus-LICENSE.txt'      = 'https://raw.githubusercontent.com/nefarius/ViGEmBus/master/LICENSE'
    'ViGEmClient-LICENSE.txt'   = 'https://raw.githubusercontent.com/nefarius/ViGEmClient/master/LICENSE'
    'HidHide-LICENSE.txt'       = 'https://raw.githubusercontent.com/nefarius/HidHide/master/LICENSE'
    'nlohmann-json-LICENSE.txt' = 'https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT'
    'Qt-LGPL-3.0.txt'           = 'https://www.gnu.org/licenses/lgpl-3.0.txt'
    'Qt-GPL-3.0.txt'            = 'https://www.gnu.org/licenses/gpl-3.0.txt'
}
foreach ($k in $texts.Keys) {
    try { Invoke-WebRequest -UseBasicParsing $texts[$k] -OutFile (Join-Path $lic $k) }
    catch { Write-Warning "could not download $k ($($texts[$k])): $($_.Exception.Message)" }
}
@"
Upgrade Aim $((Get-Content (Join-Path $repo 'installer\UpgradeAim.iss') | Select-String '#define AppVersion "(.*)"').Matches[0].Groups[1].Value)

Componentes de terceiros incluídos:
- Qt 6.8.3 (The Qt Company) — LGPL-3.0, linkado dinamicamente (DLLs Qt6*.dll nesta pasta podem ser substituídas). Código-fonte: https://download.qt.io/official_releases/qt/6.8/
- ViGEmClient (Nefarius Software Solutions) — MIT, linkado estaticamente.
- ViGEmBus 1.22.0 e HidHide 1.5.230 (Nefarius Software Solutions) — instaladores oficiais assinados, redistribuídos sem modificação.
- nlohmann/json 3.11.3 — MIT.
- Microsoft Visual C++ Runtime — redistribuível da Microsoft.
"@ | Set-Content -Encoding utf8 (Join-Path $lic 'THIRD_PARTY.txt')

# Owner-only code generator, deployed next to dist\ (never into the installer).
$kgDir = Join-Path $dist 'keygen'
Remove-Item -Recurse -Force $kgDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $kgDir | Out-Null
Copy-Item (Join-Path $repo 'build\package\src\keygen\upgradeaim-keygen.exe') $kgDir
$ErrorActionPreference = 'Continue'
& (Join-Path $qt 'bin\windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $kgDir 'upgradeaim-keygen.exe') | Out-Null
$ErrorActionPreference = 'Stop'
Remove-Item -Force (Join-Path $kgDir 'vc_redist.x64.exe') -ErrorAction SilentlyContinue

$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe', 'C:\Program Files\Inno Setup 6\ISCC.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 not found (winget install JRSoftware.InnoSetup)' }
$ErrorActionPreference = 'Continue'
& $iscc /Q (Join-Path $repo 'installer\UpgradeAim.iss')
if ($LASTEXITCODE) { throw 'ISCC failed' }
Get-ChildItem $dist -Filter 'UpgradeAim-Setup-*.exe' | Select-Object Name, @{ n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
