# Troubleshooting

Logs: `%APPDATA%\StickyAimLab\logs\app.jsonl` (app) and `cli.jsonl` (CLI), JSON Lines with monotonic timestamps. Settings → "Open data folder".

## Controller

**The controller is not listed / Dashboard says "Demo signal".**
- Run `stickyaim-cli devices`. If no XInput slot is connected, check the controller's mode switch (it must be in XInput/PC mode for the XInput source) and the cable or receiver.
- If it is listed under HID but not XInput, choose the HID entry as input source and add a device mapping ([ADDING_CONTROLLERS.md](ADDING_CONTROLLERS.md)).
- If HidHide is installed and the device is hidden, add `StickyAimLab.exe` to HidHide's allow-list.

**"Missed packets" keeps increasing.**
- The engine polls less often than the controller changes state. Use Latency Lab → Mode "Spin", or lower the interval to 125 µs (the default). At 250 µs about 70 % of the Tarantula 8K's state changes were not observed.
- A few hundred per minute in Hybrid mode is normal (scheduler wake-up jitter). It means intermediate states were skipped, not that the latest state was lost.

**New samples per second is far below the advertised polling rate.**
- XInput only reports state *changes*. A perfectly still stick produces none. Measure while moving the stick.
- The Raw Input table in Latency Lab shows HID report arrival separately. USB polling rate, HID report rate and StickyAim's processing rate are three different quantities.

**The stick drifts or the aim creeps at rest.**
- Stick Lab → Rest test (hands off) → "Apply measured center", then "Use recommended inner deadzone". The Dashboard shows the active center calibration.
- A "drift suspected" warning means the resting offset is above 0.06. The recommended deadzone still hides it, but the stick may need cleaning or service.

**The sticks on screen look slow or choppy.**
- Dashboard → "Controller read rate" shows the screen's frame rate and how old the drawn data is. The UI redraws once per monitor refresh (60/144/240 Hz). No monitor can show 8 kHz, but the engine still reads and processes every state.
- Before 2026-10-27 the Dashboard re-enumerated all HID devices on every frame (about 60 ms each), which limited the UI to about 14 fps. Device matching now runs only on start, connect/disconnect and "Rescan devices".

**Diagonals feel snapped to the axes.**
- The deadzone type is Axial. Use Radial.

## Processing

**Aiming feels delayed.**
- Check smoothing: any filter other than "Off" adds lag. Testing → Synthetic shows the exact added lag (tracking_1hz, micro_adjust). Raise the One Euro min cutoff or the flick bypass, or turn smoothing off.
- Check a crossfade time in Aim Profiles (hip ↔ ADS).
- Check the game's and Steam Input's own deadzones and curves: they stack with StickyAim's.

**Full deflection does not reach maximum turn speed.**
- Outer threshold too high for your stick's range. Run the Range sweep and apply the recommendation.
- Sensitivity < 1 on that axis.

**A profile does not load.**
- The error message names the invalid field (for example inner deadzone ≥ outer threshold). Files are validated, not silently "fixed".

## Virtual output

**"ViGEmBus driver not found".**
- The driver is not installed or not running. See [DELIVERY.md](DELIVERY.md). Device Compatibility → Drivers shows its state.

**"Virtual controller took the same XInput slot as the input source".**
- Reconnect the physical controller first, then enable virtual output, so the physical pad keeps the lower slot.

**The game reacts twice / erratically.**
- Double input: the game reads both controllers. Hide the physical controller with HidHide (allow-list StickyAim), or configure Steam Input appropriately ([COMPATIBILITY.md](COMPATIBILITY.md)).

**No rumble.**
- Rumble is forwarded from the virtual pad to the physical XInput slot. Not verified on real hardware yet. Check that the physical controller supports XInput vibration.

## Performance

**One CPU core at 100 %.**
- Spin and Hybrid at short intervals busy-wait by design. Use Timer mode (lower CPU, more jitter) or a longer interval. The trade-off is visible in Latency Lab.

**Hotkeys do not work.**
- Another program has registered Ctrl+Alt+P/X/1–9. The status bar says so at start-up.

## Build

**`vswhere.exe not found`.** Install Visual Studio 2022 or Build Tools 2022 with the C++ workload.

**`Qt 6 (msvc2022_64) not found`.** Pass `-QtRoot C:\Qt\6.8.3\msvc2022_64` or set `QT_ROOT`.

**FetchContent download fails.** The first configure needs network access to GitHub. Behind a proxy, set `HTTPS_PROXY`.

**`StickyAimLab.exe` does not start outside the build folder (missing Qt DLLs).** Build with `-Deploy`, or run `windeployqt` on the executable.
