#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stickyaim/core/types.hpp"

namespace sa {

// Deterministic synthetic input for tests, benchmarks and the preview mode.
// Every generator produces the same samples for the same parameters and seed.
struct SignalSpec {
    enum class Kind { Rest, Step, Ramp, Sine, Circle, MicroAdjust, Flick } kind = Kind::Rest;
    double rateHz = 1000.0;     // nominal sample rate
    double durationSec = 1.0;
    float amplitude = 1.0f;     // peak deflection (normalized)
    double freqHz = 1.0;        // Sine / Circle / MicroAdjust
    double stepAtSec = 0.1;     // Step / Flick
    float angleRad = 0.0f;      // direction for Step / Ramp / Sine / Flick (0 = +X)
    float noiseStd = 0.0f;      // additive Gaussian noise (normalized units)
    Vec2 offset;                // constant offset (simulated drift / off-center rest)
    float driftPerSec = 0.0f;   // linear drift along +X per second
    double timingJitterUs = 0;  // Gaussian jitter applied to timestamps
    double dropProbability = 0; // probability that a sample is skipped (sequence gap)
    std::uint32_t seed = 1;
};

// Returns right-stick samples (other axes neutral). Sequence numbers increment
// by one per generated sample, so dropped samples appear as sequence gaps.
std::vector<RawSample> generate(const SignalSpec& spec);

const char* toString(SignalSpec::Kind k) noexcept;

}  // namespace sa
