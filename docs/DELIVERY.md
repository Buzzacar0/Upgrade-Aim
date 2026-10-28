# Input delivery: preview, virtual controller, drivers and risks

## Modes

| Mode | What happens | Needs | Default |
| --- | --- | --- | --- |
| **Safe preview** | The processed state is computed, timed and visualized, but delivered nowhere. No other program can see it. | nothing | **yes** |
| **Pass-through** | The raw controller state is sent unchanged (in preview: nothing leaves the process). Use it as the panic switch and for A/B comparisons. | — | off |
| **Virtual Xbox 360 controller** | A ViGEm virtual pad is created and receives the processed state. Game rumble sent to the virtual pad is forwarded to the physical controller. | ViGEmBus driver; usually HidHide too | off; must be enabled in Settings after a risk notice |

## Account and terms-of-service risk

- Many online games forbid unauthorized software or hardware that modifies controller input. Activision's security and enforcement policy covers this for Call of Duty. Its RICOCHET anti-cheat targets input-modifying devices (Cronus Zen, XIM Matrix) and in Feb 2026 announced detections based on input timing, consistency and response patterns rather than device signatures.
- A virtual controller fed by this tool is an input-modification layer of the same category. Using it in online play may lead to enforcement against your account. StickyAim does not try to hide from, evade or interfere with anti-cheat software, and will not.
- The aiming-relevant features (deadzone, curve, sensitivity per axis, ADS response) have equivalents in most games' own controller settings and in the controller maker's official software. The safer workflow is to find the values here (measured on your controller), then set them there.

## Driver requirements

StickyAim never installs drivers, never changes security settings, and runs without administrator rights. Driver installers need administrator rights and are the user's decision.

### ViGEmBus

- Needed for: virtual controller output only.
- Status: retired upstream (archived Nov 2023), no further updates. It is a kernel-mode bus driver. Install it only if you accept running an unmaintained kernel driver.
- Install: download the final release from the author's official GitHub releases page (`nefarius/ViGEmBus`) and run the installer as administrator. Do not use re-hosted installers.
- Check: Device Compatibility → Drivers shows `ViGEmBus: running`. `stickyaim-cli devices` shows the same.
- Uninstall: Windows Settings → Apps → "Nefarius Virtual Gamepad Emulation Bus Driver".
- **Not verified during development**: the driver was not installed on the development machine, so the ViGEm sink has been compiled but never run.

### HidHide (optional, recommended with virtual output)

- Why: with a virtual pad active, a game usually sees both the physical and the virtual controller and reads both (double input). HidHide hides the physical device from all programs except allow-listed ones.
- Setup: install HidHide (administrator), open its configuration client, add `StickyAimLab.exe` (and `stickyaim-cli.exe` if used) to the application allow-list, then hide the Tarantula's device entries.
- Warning: if StickyAim is not allow-listed, it can no longer read the controller. If you hide the device and then uninstall StickyAim, unhide the device in HidHide first.
- Not verified during development.

## Safety behaviour of the output path

| Situation | Behaviour |
| --- | --- |
| Engine start with virtual output | refuses if the virtual pad would take the same XInput slot as the input source (feedback loop) |
| Physical controller disconnects | neutral report sent immediately (sticks centered, triggers and buttons released). Reconnect is retried every 250 ms. |
| Ctrl+Alt+X | virtual pad removed at once, back to preview |
| Ctrl+Alt+P / hold Back+Start 1.5 s | toggles pass-through |
| App closes normally | `Engine::stop()` sends neutral and removes the virtual pad |
| Process crashes | expected: the ViGEm bus handle is closed by Windows and the bus unplugs targets it owned. **Not verified** (no ViGEmBus on the development machine). |
| Turning off "Allow virtual output" | returns to preview immediately |

## Using the virtual output from the CLI

The CLI refuses to inject unless told explicitly:

```bash
stickyaim-cli run --profile profiles/tarantula8k_precision.json --output vigem --confirm-injection
```

Without `--confirm-injection` it prints the risk notice and exits.
