# Testing

## Automated tests

```bash
ctest --preset release
```

82 tests in two executables. All passed on 2026-10-27.

| Area (requirement) | Tests |
| --- | --- |
| Curve monotonicity and boundary conditions | `Curve.*` (all curve types, 10,000-point sweeps, NaN and out-of-domain inputs, custom-point sanitizing), `ResponseLut.*` (monotone and bounded for 16 fine/fast band combinations, zone continuity) |
| Deadzone behaviour | `Deadzone.*` (radial, axial, none; exact thresholds; continuity at the inner edge; outer saturation; validation) |
| Axis symmetry | `Pipeline.AxisSymmetry`, `Deadzone.SymmetricInAllQuadrants` (bit-exact ±X, ±Y and X↔Y) |
| Saturation and normalization | `Normalization.ExtremesAndSymmetry` (lossless int16 round trip, −32768 handling, NaN), `Pipeline.SensitivityAndCircularSaturation`, `Pipeline.OutputAlwaysWithinRange` |
| Input noise and drift | `RestAnalysis.*` (offset, noise and jitter estimates; the recommended deadzone suppresses 100 % of rest samples with and without center calibration; drift flag), `RangeAnalysis.*`, `DriftMonitor.*`, `Analysis.DeadzoneSuppressesRestNoise` |
| Sampling jitter | `Timing.MeasuresIntervalAndJitter` (√2·σ interval jitter), `Timing.DetectsDroppedSamplesAndGaps`, `Timing.NoNominalRateAssumed` |
| Device disconnection | `Engine.DisconnectNeutralizesOutputAndReconnects`, `Engine.StartsWithoutDeviceAndWaits` |
| Configuration persistence | `Config.*` (lossless JSON round trip, atomic save, tolerant load, rejection of bad files, shipped profiles load and compile), `DeviceDb.*`, `Recording.CsvRoundTrip` |
| Processing latency under load | `Latency.ProcessingCostUnderLoad` (all cores saturated), `Analysis.StaticProcessingAddsNoLatency`, `Analysis.SmoothingLagIsMeasured` (EMA lag = τ·ln2 ± 1 sample) |
| Engine behaviour | profile application, pass-through, panic combo, missed-packet counting, profile hot-swap under load (300 swaps while feeding input), telemetry timing, real-time replay |
| Smoothing | identity when off, EMA time constant and sample-rate independence, One Euro adaptivity, flick bypass, re-seeding on gaps and non-monotonic time |
| Determinism | `Pipeline.DeterministicForIdenticalInput` (bit-identical output with smoothing, crossfade, noise and timing jitter) |

## Comparing the raw and processed controller

Keep two kinds of evidence apart:

- **Objective**: what the processing does to the signal (lag, gain, noise passed through, resolution, smoothness, saturation). It is measured by StickyAim and is repeatable.
- **Subjective**: how aiming feels or performs in a game. It is affected by the game's own processing and aim assist, by learning effects and by expectation. It can only be estimated with controlled trials.

### A. Objective comparison (repeatable)

1. Testing → Synthetic comparison → choose the baseline "Raw controller (no processing)" (or another profile) → Run. The signals are seeded, so the same profile always gives identical numbers.
2. Testing → Live capture: pick a movement type, perform it, and the same recording is replayed through the active profile and the baseline. This isolates the profile's effect from variation in your hand.
3. Latency Lab: check that sample interval, missed packets and processing time are unchanged between profiles. Processing cost does not depend much on the profile (87–92 ns measured).
4. Export the JSON report.

What to look for:

| Goal | Metric |
| --- | --- |
| No drift or rest jitter reaching the game | rest_noise output RMS = 0, non-zero fraction = 0 |
| Finer micro-corrections | micro_adjust gain below 1 with output levels still high (resolution not lost) |
| No added delay | added lag 0 ms on step, tracking and flick tests; any value > 0 comes from smoothing |
| Smoother output | lower jerk RMS, but only together with acceptable added lag |
| Full speed still reachable | step_100 / full_circle gain 1.000 |

### B. Subjective comparison (blind A/B, in a private or offline mode)

1. Pick two profiles, A and B (for example *Baseline* and *Precision*). Map them to Ctrl+Alt+1 and Ctrl+Alt+2 by their order in the profile list.
2. Have someone else switch profiles between rounds without telling you which is active (Dashboard → profile list, or the hotkeys). If no one is available, use a die roll and only check the profile afterwards.
3. Use a fixed, repeatable drill: same map, same targets, same weapon and attachments, same in-game sensitivity, at least 10 rounds per profile, in alternating random order.
4. Record per round: profile (filled in afterwards), a 1–5 rating, and an objective in-game measure if available (time to kill a fixed target, hits out of N shots).
5. Write conditions and results into Testing → Subjective notes and export. The report keeps them separate from the measurements.
6. Only conclude that a profile is better if the difference is consistent across rounds and larger than the round-to-round spread.

The game's own deadzone and response-curve settings stack with StickyAim's. For comparisons with virtual output, set the game to its minimum deadzone and a linear curve if possible, and keep it unchanged between A and B.
