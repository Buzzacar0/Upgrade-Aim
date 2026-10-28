# Architecture

## Layers

```
            ┌──────────────────────── Qt UI (src/app) ─────────────────────────┐
            │ pages, curve editor, plots   ◄── telemetry (SPSC queue, 64k) ──┐ │
            └──────────────┬──────────────────────────────────────────────────┘ │
       publishProfile()    │ (pointer swap)                                    │
                           ▼                                                   │
 ┌──────────────┐   ┌───────────────────── Engine thread (src/engine) ────────┴──┐   ┌──────────────┐
 │ IInputSource │──►│ pace → poll → [panic combo] → Processor::process → submit  │──►│ IOutputSink  │
 │  XInput      │   │ disconnect → neutral output → reconnect loop                │   │  Preview     │
 │  HID (Raw In)│   └─────────────────────────────────────────────────────────────┘   │  ViGEm X360  │
 │  Replay/Demo │                                                                       └──────────────┘
 └──────────────┘
                 Processing core (src/core): portable C++20, no OS calls, fully unit-tested
```

- **Acquisition** (`IInputSource`) only reads and timestamps.
- **Processing** (`Processor`, `CompiledProfile`) is pure computation on samples and timestamps.
- **Delivery** (`IOutputSink`) only writes. Preview is the default and the only sink that cannot affect another program.

The launcher-specific part is not in the processing path at all: Steam, Battle.net and the Xbox app read whichever controller Windows exposes. They are handled by probes and documentation (`system_probe.cpp`, [COMPATIBILITY.md](COMPATIBILITY.md)), not by separate engines.

## Processing pipeline (aim stick)

| Stage | Module | Notes |
| --- | --- | --- |
| Normalize | `types.cpp` | int16 → [-1, 1] symmetric; −32768 clamps to −1 |
| Calibration | `calibration.cpp` | subtract measured center, rescale each half-axis to its measured extreme |
| Smoothing (optional) | `smoothing.cpp` | None / EMA (dt-exact) / One Euro; flick bypass above a stick speed; re-seeds on gaps > 100 ms |
| Deadzone | `deadzone.cpp` | None / Radial / Axial; inner and outer thresholds, linear rescale between them (no lost range) |
| Response | `response.cpp`, `curve.cpp` | curve + fine/fast slope bands compiled into a 4097-entry LUT |
| Sensitivity | `pipeline.cpp` | separate X/Y gains |
| Saturation | `pipeline.cpp` | circular (|v| ≤ 1) or per-axis |
| Anti-deadzone | `pipeline.cpp` | optional minimum output, to cancel a game deadzone that cannot be set to zero |

Contexts: hip-fire and ADS each have a complete configuration. ADS is entered at LT ≥ `adsEnter` and left at LT ≤ `adsExit`. An optional crossfade blends the two responses over `contextTransitionMs`.

The movement stick gets calibration and a radial deadzone. Triggers get thresholds. Buttons pass through.

### Why a LUT with integrated slope bands

Fine and fast bands are defined as **slope multipliers** over a magnitude range. The LUT is the normalized integral of (curve slope × band multiplier), so:

- it is monotone by construction (all multipliers are positive);
- full deflection still maps to full output (normalization), so a fine band never costs top-end turn speed;
- evaluation is one multiply, one truncation and one lerp, so it is deterministic and allocation-free.

The tests check monotonicity over 20,000 points for many band combinations.

### Why Fritsch–Carlson for custom curves

User-defined points are sanitized (sorted, clipped, made non-decreasing) and interpolated with a monotone cubic Hermite spline. Unlike a plain cubic spline, it cannot overshoot, so a custom curve can never reverse direction.

## Real-time engine

- One dedicated thread, MMCSS "Games" priority (fallback: `THREAD_PRIORITY_HIGHEST`).
- Pacing: `Spin`, `Hybrid` (high-resolution waitable timer, then spin for the last ≤150 µs) or `Timer`. Default is Hybrid at 125 µs. At 250 µs the engine only observed about 2.3 k of the Tarantula's ~7.6 k state changes per second.
- Late wake-ups (more than one interval late) are counted and the schedule resynchronized instead of bursting.
- **No allocation, lock or free on the input thread** after start. Profiles are published by `std::atomic` pointer exchange. The input thread pushes retired profiles into an SPSC queue, and the control thread frees them.
- Telemetry (raw, output, processing/submit/total ns, context, flags) goes into a 64k-entry SPSC queue. If it is full, the sample is dropped and counted; the input thread never waits.
- Logging from the input thread uses a separate lock-free ring (`Logger::logRt`). The writer thread turns records into JSON Lines.
- **Disconnect**: neutral state submitted at once, processor reset, reconnect attempted every 250 ms.
- **Pass-through**: bypass flag (UI, Ctrl+Alt+P, or holding Back+Start) sends the raw state unchanged.
- **Safe shutdown**: `stop()` always submits neutral, closes the sink (removing any virtual pad) and restores thread priority.
- **Idle ticks**: XInput reports nothing while the state is unchanged. If smoothing or a crossfade has not settled, the engine re-runs the last sample with a fresh timestamp so the output still converges.

## Measurement model

| Quantity | How it is measured | What it is not |
| --- | --- | --- |
| XInput sample interval | QPC at `XInputGetState` return when the packet number changes | not the USB polling interval: unchanged reports are invisible to XInput |
| Missed packets | gaps in XInput packet numbers between consecutive observations | not USB-level loss |
| HID report rate | QPC at `WM_INPUT` receipt per device (Raw Input, input sink) | includes driver and message-queue scheduling. Reports delivered in one message share a timestamp, and those messages are counted |
| Processing time | QPC around `Processor::process()` | quantized to 100 ns. The batch mean gives the precise average |
| Software path | acquisition timestamp → sink submit returned | excludes USB, driver, game and display |

The analyzers never assume a nominal rate. `TimingAnalyzer` estimates the median from data and flags gaps relative to it.

## Technical choices

- **C++20 + CMake + Ninja/MSVC**: as requested. The core also builds with `STICKYAIM_CORE_ONLY=ON` without any Windows API.
- **Qt 6 Widgets** (6.8 LTS) instead of QML: lower overhead for many live custom-painted views, simpler deployment. Plots are custom `QPainter` widgets, so Qt Charts is not needed.
- **XInput** as the primary source: the Tarantula 8K enumerates as an Xbox 360-compatible device (xusb22), and XInput exposes packet numbers for loss detection.
- **Raw Input** for report-rate measurement and for non-XInput controllers. A message-only window with `RIDEV_INPUTSINK` sees reports regardless of focus. HID parsing uses `HidP_*` with the preparsed descriptor, so axes are read generically and mapped by device files.
- **ViGEmBus** for optional output: it is still the only widely deployed user-mode-accessible virtual gamepad bus. It is retired upstream, which is why output is optional and off by default ([DELIVERY.md](DELIVERY.md)).
- **nlohmann/json** for profiles, settings, device files and reports: header-only, maintained, MIT.
- **GoogleTest** for tests.
