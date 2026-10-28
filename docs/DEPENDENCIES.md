# Dependencies

Checked 2026-10-27. "Status" is what was verified on that date, not a guarantee.

| Dependency | Version | Used for | License | Status | How it is obtained |
| --- | --- | --- | --- | --- | --- |
| nlohmann/json | 3.11.3 | profiles, settings, device files, reports | MIT | actively maintained | CMake `FetchContent`, release tarball with SHA-256 pin |
| GoogleTest | 1.15.2 | unit tests | BSD-3-Clause | actively maintained | CMake `FetchContent`, SHA-256 pin |
| Qt | 6.8.3 (LTS line) | desktop UI (Widgets only) | LGPL-3.0 / commercial | 6.8 is a long-term-support series | installed separately (`aqtinstall` or the Qt installer) |
| ViGEmClient | final upstream commit `b66d02d` (2023-09-08) | optional virtual Xbox 360 output | MIT | **retired**: the repository was archived by its author in Nov 2023 and gets no fixes | CMake `FetchContent` from GitHub, pinned commit. Built as a static library. |
| ViGEmBus (driver, runtime) | not bundled | needed at runtime for virtual output only | BSD-3-Clause | **retired** with ViGEmClient. The last release still installs and works on Windows 10/11 according to its users, but this was **not verified here**. | installed by the user (never by StickyAim) |
| HidHide (driver, runtime, optional) | not bundled | hiding the physical controller from games | MIT | same author as ViGEm; check its status before use | installed by the user (never by StickyAim) |
| Windows SDK APIs | 10.0.26100 | XInput 1.4, Raw Input, HID (`hid.lib`), SetupAPI, MMCSS (`avrt`), waitable timers | — | part of Windows | Windows SDK |

## Notes on the virtual-controller stack

- The upstream author has mentioned a successor project. At the time of checking, no public, documented replacement with a stable API was available, so StickyAim keeps ViGEm behind an interface (`IOutputSink`). A future backend only needs a new sink.
- Because both the client and the bus are retired, the virtual output is optional at build time (`STICKYAIM_WITH_VIGEM`) and off at run time until the user enables it in Settings.

## Not used, and why

- **Qt Charts**: custom `QPainter` widgets are lighter for high-rate live data and avoid a GPL/commercial-only module.
- **SDL / DirectInput**: XInput plus Raw Input covers the target hardware with direct access to packet numbers and HID reports.
- **Kernel drivers of our own**: out of scope. They would need signing, and installing them would mean modifying the system.
