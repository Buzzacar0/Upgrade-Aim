# Benchmarks

All numbers were measured on 2026-10-27 on the development machine (Windows 11 Pro 10.0.26200, x64, AMD platform, MSVC 2022, RelWithDebInfo build) with a **GameSir Tarantula 8K** over USB in XInput mode. Re-run them on your machine with the commands given; numbers from another machine do not transfer.

## 1. Device timing (CLI `measure`, sticks untouched, 5 s, spin polling)

```bash
stickyaim-cli measure --seconds 5 --mode spin
```

| Quantity | n | mean | sd | p50 | p95 | p99 | max |
| --- | --- | --- | --- | --- | --- | --- | --- |
| XInput state-change interval | 37,878 | 132.0 µs (7,576 /s) | 33.7 µs | 124.5 µs | 169.5 µs | 258.5 µs | 426.9 µs |
| Raw Input HID report interval (IG_00) | 37,878 | 132.0 µs (7,576 /s) | 37.9 µs | 124.5 µs | 225.5 µs | 277.5 µs | 465.5 µs |
| `XInputGetState()` call cost | 3.0 M | 1.61 µs | — | — | — | 2.75 µs | 179.5 µs |

- Packet numbers skipped between polls: 0. HID messages carrying more than one report: 3 of 37,876.
- The packet number advances at rest because the stick values jitter by about ±20 raw counts on every report. Measure with the stick moving as well: a controller with a quieter sensor would show far fewer XInput changes at rest, and that would not mean a lower polling rate.

## 2. Rest noise (CLI `calibrate`)

```bash
stickyaim-cli calibrate --rest 4 --range 6 --out cal.json
```

| Stick | n | mean offset | sd | max \|v\| | max \|v − mean\| | recommended inner DZ (uncal / cal) |
| --- | --- | --- | --- | --- | --- | --- |
| Right | 258 | (+0.0140, +0.0062) | (0.0002, 0.0002) | 0.0159 | 0.0007 | 0.019 / 0.010 |
| Left | 258 | (+0.0060, +0.0296) | (0.0002, 0.0002) | 0.0308 | 0.0007 | 0.037 / 0.010 |

This run used an earlier capture loop that slept about 15 ms between polls, which is why n is only 258. The capture now spin-polls. The recommended values include a 20 % margin over the largest observed excursion, with a 0.010 floor.

## 3. Engine (CLI `run`, preview sink, hybrid 125 µs, stick moved by hand, 6 s)

```bash
stickyaim-cli run --profile profiles/tarantula8k_precision.json --seconds 6 --mode hybrid --interval 125
```

| Quantity | Result |
| --- | --- |
| New states processed | ≈ 7,400 /s (44,435 in 6 s) |
| Device packets not observed by the loop | 873 cumulative (≈ 2 %; ≈ 0.3 % over the first 4 s) |
| Late wake-ups (> 1 interval late) | 1 |
| `Processor::process()` | p50 0.21 µs, p99 1.21 µs |
| Sink submit (preview) | p50 0.1 µs, p99 0.2 µs |
| Acquisition → submit complete | p99 1.5 µs |

Same machine, earlier session, default interval 250 µs: ≈ 2,300 new states/s, and most of the device's state changes were not observed. That is why the default is now 125 µs. Spin mode observes every packet the driver exposes at the cost of one busy core.

The ViGEm sink's submit cost (a `DeviceIoControl` per sample) was **not measured** (no ViGEmBus installed).

## 4. Processing cost (synthetic, `benchmarkProcessing`)

| Condition | Batch mean per sample | per-sample p50 / p99 |
| --- | --- | --- |
| Idle machine, shipped profiles | 87–92 ns | 102 / 202 ns (QPC quantization: 100 ns) |
| Every core saturated by other threads (unit test `Latency.ProcessingCostUnderLoad`) | 386 ns | 102 / 202 ns; max 18.9 ms (pre-emption of a normal-priority test thread) |

## 5. Synthetic comparison suite (CLI `compare`, 1000 Hz, hip context)

```bash
stickyaim-cli compare --profile profiles/<name>.json --context hip --out report.json
```

Full JSON reports are in [benchmarks/](benchmarks/). Added lag is time-to-50 % (steps) or cross-correlation delay (sine and micro-adjust) relative to the raw signal. Gain ratio is RMS(output) / RMS(input) along the test direction.

| Test | Baseline | Precision | Custom S-curve | Smooth tracking (hip) | Smooth tracking (ADS) |
| --- | --- | --- | --- | --- | --- |
| rest_noise: output RMS (raw 0.0257) | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 |
| step_50: added lag | 0 ms | 0 ms | 0 ms | 0 ms (flick bypass) | 0 ms |
| tracking_1hz: added lag / gain | 0 ms / 0.922 | 0 ms / 0.606 | 0 ms / 0.538 | **12 ms** / 0.477 | **12 ms** / 0.349 |
| micro_adjust: added lag / gain | 0 ms / 0.528 | 0 ms / 0.163 | 0 ms / 0.142 | **13 ms** / 0.188 | **13 ms** / 0.115 |
| flick_left: added lag | 0 ms | 0 ms | 0 ms | 0 ms | 0 ms |
| micro_adjust: output jerk RMS (raw 5.0e-3) | 4.2e-3 | 1.8e-3 | 1.3e-3 | 8.6e-5 | 5.3e-5 |

Reading the table:

- Static shaping (deadzone, curve, bands, sensitivity) adds **zero** latency: the output changes on the same sample as the input.
- Convex curves lower the gain at small deflections (micro-adjust gain 0.14–0.16 vs 0.53 linear). That means more stick travel per unit of output, which is the intended precision trade-off. It does not mean the controller is "slower" at full deflection: step_100 and full_circle stay at gain 1.000.
- Smoothing is the only feature that adds latency. The first version of the Smooth-tracking profile added 37–38 ms. It was retuned to 12–13 ms. Given the low measured rest jitter of this controller, compare it against the Precision profile before keeping it.
- The baseline's tracking jerk is higher than raw (1.2e-4 vs 2.5e-5) because the deadzone creates a kink at each zero crossing. This is a real, small artifact of any deadzone.

## Reproducing

1. Build in release (`scripts/build.ps1`).
2. Run the four commands above with your controller connected.
3. Run `ctest --preset release -R Latency -V` for the load test.
4. Compare your JSON reports with the ones in [benchmarks/](benchmarks/). Synthetic results must match to the last digit (seeded signals). Device and timing results depend on hardware and load.
