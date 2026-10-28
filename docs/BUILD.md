# Build and installation

## Requirements

| Component | Version used and verified | Notes |
| --- | --- | --- |
| Windows | 11 Pro 10.0.26200 (x64) | Windows 10 1803+ is needed for high-resolution waitable timers. Older builds fall back to normal timers. |
| Visual Studio 2022 or Build Tools 2022 | Build Tools 2022, MSVC x64 | Workload "Desktop development with C++" / `Microsoft.VisualStudio.Workload.VCTools` |
| Windows SDK | 10.0.26100 | installed with the workload |
| CMake | 3.24 or newer | the VS-bundled CMake works |
| Ninja | any recent | bundled with VS, or `winget install Ninja-build.Ninja` |
| Qt | 6.8.3, `msvc2022_64` | only needed for the desktop app. Any Qt ≥ 6.5 should work. |
| Git | any | CMake fetches ViGEmClient from GitHub |

GoogleTest, nlohmann/json and ViGEmClient are downloaded by CMake at configure time (pinned versions and hashes, see [DEPENDENCIES.md](DEPENDENCIES.md)). Network access is needed for the first configure.

## Installing the toolchain (what was done on the development machine)

Run from an elevated prompt; every step installs software system-wide.

```bash
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project"
```

```bash
winget install --id Kitware.CMake
```

```bash
winget install --id Ninja-build.Ninja
```

Qt through the unofficial `aqtinstall` downloader (or use the official Qt online installer):

```bash
python -m pip install aqtinstall
```

```bash
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt
```

## Building

The script finds Visual Studio, imports the x64 developer environment, finds Qt under `C:\Qt\6.*\msvc2022_64` (or uses `-QtRoot` / `QT_ROOT`), then configures, builds and tests.

```bash
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

Options:

| Option | Effect |
| --- | --- |
| `-Preset release` | default: app + CLI + tests, RelWithDebInfo |
| `-Preset debug` | debug build |
| `-Preset core` | portable core and its tests only (no Qt, no Windows APIs) |
| `-QtRoot <dir>` | Qt kit directory |
| `-NoTest` | skip ctest |
| `-Deploy` | run `windeployqt` so `StickyAimLab.exe` runs outside the build environment |

Manual equivalent from a "x64 Native Tools Command Prompt for VS 2022":

```bash
cmake --preset release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
```

```bash
cmake --build --preset release
```

```bash
ctest --preset release
```

CMake options: `STICKYAIM_BUILD_APP`, `STICKYAIM_BUILD_CLI`, `STICKYAIM_BUILD_TESTS`, `STICKYAIM_WITH_VIGEM` (compile the optional virtual-controller sink, default ON), `STICKYAIM_CORE_ONLY`.

## Running

- App: `build/release/src/app/StickyAimLab.exe`. It starts in safe preview with the first connected XInput controller, or with the demo signal if none is connected.
- CLI: `build/release/src/cli/stickyaim-cli.exe help`.
- User data (settings, calibration, user profiles, logs): `%APPDATA%\StickyAimLab`.
- `StickyAimLab.exe --screenshots <dir>` renders every page to PNG and exits (used for the screenshots in this repository).

No administrator rights are needed to build or run StickyAim Lab. The optional third-party drivers in [DELIVERY.md](DELIVERY.md) need them to install.

## Installing

```bash
cmake --install build/release --prefix "C:/Program Files/StickyAimLab"
```

This installs the executables to `bin/` and the profiles and device files to `share/stickyaim/`, where the app finds them. Run `windeployqt` on the installed `StickyAimLab.exe` (or build with `-Deploy`).
