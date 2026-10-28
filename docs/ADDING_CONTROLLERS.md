# Adding support for another controller

## 1. XInput controllers: nothing to do

If the controller appears in an XInput slot (`stickyaim-cli devices` lists it under "XInput slots"), the XInput source already works. A device file only adds a name, notes and a HID mapping.

## 2. Identify the device

```bash
stickyaim-cli devices
```

Note the `VID_xxxx PID_yyyy` of the gamepad/joystick interface, whether it is marked `[XInput HID view]`, and the product string. Different controller modes (XInput, DirectInput, Switch, wireless dongle) usually have different PIDs. List only those you observe.

## 3. Measure it

```bash
stickyaim-cli measure --seconds 10
```

Move the stick continuously. Record the XInput and HID rates and intervals.

```bash
stickyaim-cli calibrate --rest 4 --range 6
```

Record rest offset and noise.

## 4. Write a device file

Copy `devices/TEMPLATE.json.txt` to `devices/<vendor>_<model>.json`:

```json
{
  "name": "Vendor Model",
  "vendor_id": "1234",
  "modes": [
    { "product_id": "ABCD", "mode": "DirectInput", "connection": "USB",
      "verification": "Observed 2026-10-27 on Windows 11 26200: <what you saw, with numbers>" }
  ],
  "notes": ["Facts you measured."],
  "unverified": ["Claims you could not measure."],
  "hid_mapping": {
    "preset": "generic",
    "lx": "X", "ly": "-Y", "rx": "Z", "ry": "-Rz", "lt": "Rx", "rt": "Ry",
    "buttons": { "1": "X", "2": "A", "3": "B", "4": "Y", "5": "LB", "6": "RB", "9": "Back", "10": "Start", "11": "LS", "12": "RS" },
    "hat_to_dpad": true,
    "verified": false
  }
}
```

Rules:

- Leave `verification` empty unless you actually observed that mode. Leave `"verified": false` on the mapping until every axis and button has been checked in the Dashboard.
- Axis specs: `X Y Z Rx Ry Rz Slider Dial`, optional leading `-` to invert, optional trailing `+` or `-` to use only the positive or negative half of a centered axis (combined trigger axes). `none` disables the target.
- Presets: `xinput_hid` (Windows' HID view of XInput pads: X/Y, Rx/Ry, Z = combined triggers) or `generic` (common DirectInput layout). Explicit keys override the preset.
- Button names: `A B X Y LB RB Back Start LS RS Guide DpadUp DpadDown DpadLeft DpadRight`. Numbers are HID button usages (1-based).

## 5. Verify the mapping

1. Start the app → Dashboard → Input source → choose the HID entry for your device.
2. Move each stick and trigger and press each button. Check the Dashboard sticks and Stick Lab plots: up must be +Y and right must be +X.
3. If an axis is inverted or swapped, fix the spec and click "Rescan devices".
4. When everything matches, set `"verified": true` and describe what you checked in `verification`.

## 6. Add a test (optional but encouraged)

`tests/test_config.cpp` → `DeviceDb.ShippedDevicesLoad` shows how to assert that a device file loads and matches its VID/PID.

## 7. New source types

To support a new API (for example GameInput), implement `sa::IInputSource` (`src/core/include/stickyaim/core/io.hpp`):

- `open()` returns false while the device is absent (the engine retries).
- `poll()` must not block or allocate. Return `NewSample` only for new data, and fill `RawSample::sequence` if the API gives one (then return true from `hasDeviceSequence()`, so missed packets are counted).
- Add it to `AppController::makeSource()`.

The processing core, engine, telemetry and all pages work unchanged.
