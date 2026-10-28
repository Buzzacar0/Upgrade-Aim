# Compatibility

Status legend: **Verified** = observed on real hardware during development, with the evidence listed. **Unverified** = implemented or plausible, but not observed. Nothing here is inferred from marketing material.

## Controllers

### GameSir Tarantula 8K

| Item | Status | Evidence / notes |
| --- | --- | --- |
| Enumeration in XInput mode | **Verified** | `VID_3537 PID_103D`, "Xbox 360 Controller for Windows" (xusb22), XInput slot 0. Interfaces: `IG_00` (HID gamepad, the XInput HID view), `MI_01` (consumer control + vendor page 0xFFF0). `stickyaim-cli devices`, 2026-10-27, Windows 11 Pro 26200. |
| XInput state-change rate | **Verified** | 7,576 changes/s with sticks untouched (5 s, spin polling), mean interval 132.0 µs, p50 124.5 µs |
| HID report rate (Raw Input, IG_00) | **Verified** | 7,576 reports/s in the same run, mean 132.0 µs, p99 277.5 µs |
| Rest noise | **Verified** | about ±20 raw counts on every report (sd ≈ 0.0002 normalized). This is why the packet number advances even at rest. |
| Rest offsets (test unit) | **Verified for that unit** | right stick (+0.0140, +0.0062), left stick (+0.0060, +0.0296). Re-measure your own unit. |
| Live processing in the app (preview) | **Verified** | Dashboard, Stick Lab, Latency Lab and Testing with the stick moved by hand; engine saw ≈ 7.4 k new states/s at 125 µs hybrid polling |
| Connection type (cable vs 2.4 GHz receiver) | Unverified | both enumerate as USB, so the link used in testing was not identified |
| Other modes (Switch, DirectInput/Android, Bluetooth) | Unverified | their product IDs were not observed, so they are not in `devices/` |
| Raw Input source (HID mapping) for this controller | Unverified | decoding uses the generic XInput-HID mapping (`hid_mapping.verified = false`) |
| Rumble forwarding through the virtual pad | Unverified | needs ViGEmBus |
| Interaction with GameSir firmware/app settings | Unverified | firmware-side deadzones or curves stack with StickyAim processing |

### Other controllers

- **Any XInput controller** works with the XInput source without configuration. Not tested here.
- **HID / DirectInput controllers** work with the Raw Input source and a device file with a mapping. Not tested here. See [ADDING_CONTROLLERS.md](ADDING_CONTROLLERS.md).

## Launchers

The processing core is identical for every launcher. Launchers differ only in which controller the game ends up reading. StickyAim detects launchers with the probes below. **No launcher or game was tested with StickyAim's output during development**, and the in-app checklist ships with every entry set to "Untested".

| Launcher | Detection (how) | On the development machine | Tested with a game |
| --- | --- | --- | --- |
| Steam | `HKCU\Software\Valve\Steam\SteamPath`, fallback `HKLM\…\WOW6432Node\Valve\Steam`; process `steam.exe` | installed, running | no |
| Battle.net | `Uninstall\Battle.net` or `HKLM\…\Blizzard Entertainment\Battle.net`; processes `Battle.net.exe`, `Agent.exe` | not installed | no |
| Xbox app / Microsoft Store | `Microsoft.GamingApp_*` package registered for the user; process `XboxPcApp.exe` | not installed | no |

### Known interaction points (to test, not results)

**Steam**
- Steam Input is set per game (Game → Properties → Controller). When enabled, Steam reads the physical controller itself and presents its own virtual controller to the game. Its deadzones and curves then stack with StickyAim's.
- Steam Input can also pick up the StickyAim virtual pad. Test with Steam Input on and off for the game.

**Battle.net**
- Battle.net does not remap controllers. The game reads controllers directly. The main risk is double input (physical and virtual pad both visible).

**Xbox app / Microsoft Store**
- Store and Game Pass builds read controllers through XInput/GameInput. Check that the virtual pad is visible to the game, not only to the Xbox app.

### Per-launcher test procedure

Do this in an offline or private mode first. Record each result in **Device Compatibility → Manual checklist**, then export the report.

1. **Physical controller only** (StickyAim in preview): does the game react? With Steam, test Steam Input off and on.
2. **Virtual pad active, physical not hidden**: does the game react to the virtual pad, to the physical one, or to both? Moving only the right stick in StickyAim's pass-through vs processed modes shows which controller the game uses.
3. **Virtual pad active, physical hidden** (HidHide with StickyAim allow-listed): is the double input gone?
4. **Rumble**: do in-game vibration events reach the physical controller?
5. **Disconnect**: unplug the physical controller during play. The virtual pad must center at once (the engine submits a neutral state), and reconnecting must resume.
6. Record the launcher version, game build, Windows build and StickyAim profile with each result.
