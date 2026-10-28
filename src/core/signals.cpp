#include "stickyaim/core/signals.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace sa {
namespace {
constexpr double kTwoPi = 6.283185307179586;
}

std::vector<RawSample> generate(const SignalSpec& spec) {
    std::vector<RawSample> out;
    const double rate = std::max(1.0, spec.rateHz);
    const auto n = static_cast<std::size_t>(std::max(0.0, spec.durationSec) * rate);
    out.reserve(n);

    std::mt19937 rng(spec.seed);
    std::normal_distribution<float> noise(0.0f, 1.0f);
    std::normal_distribution<double> jitter(0.0, 1.0);
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    const float dx = std::cos(spec.angleRad), dy = std::sin(spec.angleRad);
    const double periodNs = 1e9 / rate;
    TimestampNs lastT = -1;

    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / rate;
        float m = 0.0f;
        Vec2 v;
        switch (spec.kind) {
            case SignalSpec::Kind::Rest:
                break;
            case SignalSpec::Kind::Step:
                m = t >= spec.stepAtSec ? spec.amplitude : 0.0f;
                v = {dx * m, dy * m};
                break;
            case SignalSpec::Kind::Ramp:
                m = static_cast<float>(spec.amplitude * std::min(1.0, t / std::max(1e-9, spec.durationSec)));
                v = {dx * m, dy * m};
                break;
            case SignalSpec::Kind::Sine:
                m = static_cast<float>(spec.amplitude * std::sin(kTwoPi * spec.freqHz * t));
                v = {dx * m, dy * m};
                break;
            case SignalSpec::Kind::Circle:
                v = {static_cast<float>(spec.amplitude * std::cos(kTwoPi * spec.freqHz * t)),
                     static_cast<float>(spec.amplitude * std::sin(kTwoPi * spec.freqHz * t))};
                break;
            case SignalSpec::Kind::MicroAdjust:
                // Small corrections around a slow tracking motion.
                m = static_cast<float>(spec.amplitude * (0.5 * std::sin(kTwoPi * spec.freqHz * t) +
                                                         0.5 * std::sin(kTwoPi * spec.freqHz * 3.1 * t)));
                v = {dx * m, dy * m};
                break;
            case SignalSpec::Kind::Flick:
                // Fast full-deflection move held for 80 ms, then release.
                m = (t >= spec.stepAtSec && t < spec.stepAtSec + 0.08) ? spec.amplitude : 0.0f;
                v = {dx * m, dy * m};
                break;
        }
        v = v + spec.offset;
        v.x += static_cast<float>(spec.driftPerSec * t);
        if (spec.noiseStd > 0.0f) {
            v.x += noise(rng) * spec.noiseStd;
            v.y += noise(rng) * spec.noiseStd;
        }
        const bool drop = spec.dropProbability > 0.0 && uni(rng) < spec.dropProbability;
        double tNs = static_cast<double>(i) * periodNs;
        if (spec.timingJitterUs > 0.0) tNs += jitter(rng) * spec.timingJitterUs * 1000.0;
        if (drop) continue;

        RawSample s;
        s.t = std::max(lastT + 1, static_cast<TimestampNs>(std::llround(tNs)));
        lastT = s.t;
        s.sequence = static_cast<std::uint32_t>(i + 1);
        s.pad.rx = denormalizeAxis(v.x);
        s.pad.ry = denormalizeAxis(v.y);
        out.push_back(s);
    }
    return out;
}

const char* toString(SignalSpec::Kind k) noexcept {
    switch (k) {
        case SignalSpec::Kind::Rest: return "rest";
        case SignalSpec::Kind::Step: return "step";
        case SignalSpec::Kind::Ramp: return "ramp";
        case SignalSpec::Kind::Sine: return "sine";
        case SignalSpec::Kind::Circle: return "circle";
        case SignalSpec::Kind::MicroAdjust: return "micro_adjust";
        case SignalSpec::Kind::Flick: return "flick";
    }
    return "rest";
}

}  // namespace sa
