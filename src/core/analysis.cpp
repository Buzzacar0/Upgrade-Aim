#include "stickyaim/core/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include <nlohmann/json.hpp>

#include "stickyaim/core/clock.hpp"
#include "stickyaim/core/stats.hpp"

namespace sa {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Series {
    std::vector<double> t;     // seconds
    std::vector<double> raw;   // raw projected on the test direction
    std::vector<double> proc;  // processed projected on the test direction
    std::vector<int> rawLevel, procLevel;
    std::vector<double> procMag;
};

// Forces the processor into a given context by overriding the left trigger.
RawSample withContext(RawSample s, ContextId ctx, const Profile& p) {
    if (p.adsFromLeftTrigger) s.pad.leftTrigger = ctx == ContextId::Ads ? 255 : 0;
    return s;
}

Series run(const CompiledProfile& profile, ContextId ctx, const std::vector<RawSample>& samples, float angle,
           bool projectOnMagnitude) {
    Processor proc;
    proc.setProfile(&profile);
    Series s;
    const float dx = std::cos(angle), dy = std::sin(angle);
    const TimestampNs t0 = samples.empty() ? 0 : samples.front().t;
    for (const auto& in : samples) {
        const RawSample rs = withContext(in, ctx, profile.profile);
        const GamepadState out = proc.process(rs);
        const Vec2 r{normalizeAxis(rs.pad.rx), normalizeAxis(rs.pad.ry)};
        s.t.push_back(static_cast<double>(in.t - t0) * 1e-9);
        if (projectOnMagnitude) {
            s.raw.push_back(length(r));
            s.proc.push_back(length(out.right));
        } else {
            s.raw.push_back(r.x * dx + r.y * dy);
            s.proc.push_back(out.right.x * dx + out.right.y * dy);
        }
        const RawGamepad o = denormalize(out);
        s.rawLevel.push_back(rs.pad.rx * 65536 + rs.pad.ry);
        s.procLevel.push_back(o.rx * 65536 + o.ry);
        s.procMag.push_back(length(out.right));
    }
    return s;
}

double firstCrossing(const Series& s, const std::vector<double>& v, double threshold, double after) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (s.t[i] >= after && v[i] >= threshold) return s.t[i] * 1000.0;
    return kNaN;
}

double rms(const std::vector<double>& v) {
    if (v.empty()) return 0;
    double acc = 0;
    for (double x : v) acc += x * x;
    return std::sqrt(acc / static_cast<double>(v.size()));
}

double jerkRms(const std::vector<double>& v) {
    if (v.size() < 3) return 0;
    double acc = 0;
    for (std::size_t i = 2; i < v.size(); ++i) {
        const double d2 = v[i] - 2 * v[i - 1] + v[i - 2];
        acc += d2 * d2;
    }
    return std::sqrt(acc / static_cast<double>(v.size() - 2));
}

// Delay of `b` relative to `a`, by maximizing normalized cross-correlation
// over non-negative shifts up to maxShift samples.
double crossCorrelationLagSamples(const std::vector<double>& a, const std::vector<double>& b, int maxShift) {
    int best = 0;
    double bestScore = -1e300;
    const int n = static_cast<int>(std::min(a.size(), b.size()));
    for (int k = 0; k <= maxShift && k < n / 2; ++k) {
        double sab = 0, saa = 0, sbb = 0;
        for (int i = 0; i + k < n; ++i) {
            sab += a[i] * b[i + k];
            saa += a[i] * a[i];
            sbb += b[i + k] * b[i + k];
        }
        const double score = (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0;
        if (score > bestScore + 1e-12) {
            bestScore = score;
            best = k;
        }
    }
    return best;
}

int distinct(const std::vector<int>& v) { return static_cast<int>(std::set<int>(v.begin(), v.end()).size()); }

void fillCommon(SignalTestResult& r, const Series& s) {
    r.gainRatio = rms(s.raw) > 0 ? rms(s.proc) / rms(s.raw) : 0;
    r.rawDistinctLevels = distinct(s.rawLevel);
    r.procDistinctLevels = distinct(s.procLevel);
    r.rawJerkRms = jerkRms(s.raw);
    r.procJerkRms = jerkRms(s.proc);
    std::size_t nz = 0, sat = 0;
    for (double m : s.procMag) {
        if (m > 1e-6) ++nz;
        if (m >= 0.999) ++sat;
    }
    const double n = static_cast<double>(std::max<std::size_t>(1, s.procMag.size()));
    r.procNonZeroFraction = static_cast<double>(nz) / n;
    r.procSaturatedFraction = static_cast<double>(sat) / n;
}

}  // namespace

SignalTestResult measureSignal(const CompiledProfile& profile, ContextId ctx, const std::string& name,
                               const std::string& description, const SignalSpec& spec) {
    SignalTestResult r;
    r.name = name;
    r.description = description;
    r.spec = spec;
    r.rawT50Ms = r.procT50Ms = r.addedLagMs = r.rawT90Ms = r.procT90Ms = kNaN;
    r.rawRestRms = r.procRestRms = kNaN;

    const auto samples = generate(spec);
    const bool magnitude = spec.kind == SignalSpec::Kind::Rest || spec.kind == SignalSpec::Kind::Circle;
    const Series s = run(profile, ctx, samples, spec.angleRad, magnitude);
    fillCommon(r, s);

    switch (spec.kind) {
        case SignalSpec::Kind::Step:
        case SignalSpec::Kind::Flick: {
            const double finalRaw = spec.amplitude;
            // Final processed value: steady-state of the same input through the
            // profile with smoothing removed (static response).
            double finalProc = 0;
            for (std::size_t i = 0; i < s.proc.size(); ++i)
                if (s.t[i] >= spec.stepAtSec) finalProc = std::max(finalProc, s.proc[i]);
            r.rawT50Ms = firstCrossing(s, s.raw, 0.5 * finalRaw, spec.stepAtSec) - spec.stepAtSec * 1000.0;
            r.rawT90Ms = firstCrossing(s, s.raw, 0.9 * finalRaw, spec.stepAtSec) - spec.stepAtSec * 1000.0;
            if (finalProc > 0) {
                r.procT50Ms = firstCrossing(s, s.proc, 0.5 * finalProc, spec.stepAtSec) - spec.stepAtSec * 1000.0;
                r.procT90Ms = firstCrossing(s, s.proc, 0.9 * finalProc, spec.stepAtSec) - spec.stepAtSec * 1000.0;
                r.addedLagMs = r.procT50Ms - r.rawT50Ms;
            }
            break;
        }
        case SignalSpec::Kind::Sine:
        case SignalSpec::Kind::MicroAdjust: {
            const int maxShift = static_cast<int>(spec.rateHz * 0.05);  // search up to 50 ms
            r.addedLagMs = crossCorrelationLagSamples(s.raw, s.proc, maxShift) / spec.rateHz * 1000.0;
            break;
        }
        case SignalSpec::Kind::Rest:
            r.rawRestRms = rms(s.raw);
            r.procRestRms = rms(s.proc);
            break;
        default:
            break;
    }
    return r;
}

ComparisonReport runSyntheticComparison(const CompiledProfile& profile, ContextId ctx, double rateHz) {
    ComparisonReport rep;
    rep.profileName = profile.profile.name;
    rep.context = toString(ctx);
    rep.rateHz = rateHz;

    SignalSpec base;
    base.rateHz = rateHz;

    SignalSpec rest = base;
    rest.kind = SignalSpec::Kind::Rest;
    rest.durationSec = 2.0;
    rest.noiseStd = 0.004f;
    rest.offset = {0.02f, -0.015f};
    rep.tests.push_back(measureSignal(profile, ctx, "rest_noise", "Untouched stick with sensor noise and a small off-center rest", rest));

    SignalSpec drift = rest;
    drift.offset = {0.0f, 0.0f};
    drift.driftPerSec = 0.02f;
    drift.durationSec = 3.0;
    rep.tests.push_back(measureSignal(profile, ctx, "slow_drift", "Rest position drifting 0.02/s along +X", drift));

    SignalSpec step = base;
    step.kind = SignalSpec::Kind::Step;
    step.durationSec = 0.4;
    step.amplitude = 0.5f;
    rep.tests.push_back(measureSignal(profile, ctx, "step_50", "Instant 50% deflection (latency added by processing)", step));

    SignalSpec stepFull = step;
    stepFull.amplitude = 1.0f;
    rep.tests.push_back(measureSignal(profile, ctx, "step_100", "Instant full deflection", stepFull));

    SignalSpec track = base;
    track.kind = SignalSpec::Kind::Sine;
    track.durationSec = 3.0;
    track.amplitude = 0.35f;
    track.freqHz = 1.0;
    rep.tests.push_back(measureSignal(profile, ctx, "tracking_1hz", "Smooth 1 Hz horizontal tracking at 35% deflection", track));

    SignalSpec micro = base;
    micro.kind = SignalSpec::Kind::MicroAdjust;
    micro.durationSec = 3.0;
    micro.amplitude = 0.12f;
    micro.freqHz = 0.7;
    micro.noiseStd = 0.002f;
    rep.tests.push_back(measureSignal(profile, ctx, "micro_adjust", "Small corrections (<=12% deflection) with noise", micro));

    SignalSpec flick = base;
    flick.kind = SignalSpec::Kind::Flick;
    flick.durationSec = 0.4;
    flick.amplitude = 1.0f;
    flick.angleRad = 3.14159265f;
    rep.tests.push_back(measureSignal(profile, ctx, "flick_left", "80 ms full-deflection flick to the left", flick));

    SignalSpec circle = base;
    circle.kind = SignalSpec::Kind::Circle;
    circle.durationSec = 2.0;
    circle.amplitude = 1.0f;
    circle.freqHz = 1.0;
    rep.tests.push_back(measureSignal(profile, ctx, "full_circle", "Stick rotated around the rim at 1 rev/s", circle));

    rep.cost = benchmarkProcessing(profile, 100000);
    return rep;
}

ProcessingCost benchmarkProcessing(const CompiledProfile& profile, std::size_t n) {
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::Circle;
    spec.rateHz = 8000.0;
    spec.durationSec = static_cast<double>(std::max<std::size_t>(n, 1000)) / spec.rateHz;
    spec.amplitude = 0.8f;
    spec.freqHz = 2.0;
    spec.noiseStd = 0.003f;
    auto samples = generate(spec);
    for (std::size_t i = 0; i < samples.size(); ++i) samples[i].pad.leftTrigger = (i / 4000) % 2 ? 255 : 0;

    Processor proc;
    proc.setProfile(&profile);
    Histogram h(5.0, 20000);  // 5 ns bins up to 100 us
    volatile float sink = 0;
    for (std::size_t i = 0; i < std::min<std::size_t>(samples.size(), 2000); ++i) sink = sink + proc.process(samples[i]).right.x;
    proc.reset();
    for (const auto& s : samples) {
        const TimestampNs a = nowNs();
        const GamepadState out = proc.process(s);
        const TimestampNs b = nowNs();
        sink = sink + out.right.x;
        h.add(static_cast<double>(b - a));
    }
    proc.reset();
    const TimestampNs batchStart = nowNs();
    for (const auto& s : samples) sink = sink + proc.process(s).right.x;
    const TimestampNs batchEnd = nowNs();

    // Smallest non-zero step of the clock, observed.
    TimestampNs resolution = 0;
    for (int i = 0; i < 256; ++i) {
        const TimestampNs a = nowNs();
        TimestampNs b = a;
        while (b == a) b = nowNs();
        if (resolution == 0 || b - a < resolution) resolution = b - a;
    }

    ProcessingCost c;
    c.batchMeanNs = static_cast<double>(batchEnd - batchStart) / static_cast<double>(std::max<std::size_t>(1, samples.size()));
    c.timerResolutionNs = static_cast<double>(resolution);
    c.samples = h.count();
    c.meanNs = h.mean();
    c.p50Ns = h.percentile(50);
    c.p99Ns = h.percentile(99);
    c.p999Ns = h.percentile(99.9);
    c.maxNs = h.max();
    return c;
}

SignalTestResult analyzeRecording(const CompiledProfile& profile, const std::vector<RawSample>& rec, const std::string& name) {
    SignalTestResult r;
    r.name = name;
    r.description = "Recorded from device and replayed through the profile";
    r.rawT50Ms = r.procT50Ms = r.addedLagMs = r.rawT90Ms = r.procT90Ms = kNaN;
    Processor proc;
    proc.setProfile(&profile);
    Series s;
    const TimestampNs t0 = rec.empty() ? 0 : rec.front().t;
    std::vector<double> rawX, procX;
    for (const auto& in : rec) {
        const GamepadState out = proc.process(in);
        const Vec2 rv{normalizeAxis(in.pad.rx), normalizeAxis(in.pad.ry)};
        s.t.push_back(static_cast<double>(in.t - t0) * 1e-9);
        s.raw.push_back(length(rv));
        s.proc.push_back(length(out.right));
        rawX.push_back(rv.x);
        procX.push_back(out.right.x);
        const RawGamepad o = denormalize(out);
        s.rawLevel.push_back(in.pad.rx * 65536 + in.pad.ry);
        s.procLevel.push_back(o.rx * 65536 + o.ry);
        s.procMag.push_back(length(out.right));
    }
    fillCommon(r, s);
    // Lag estimate only makes sense with roughly uniform sampling; derive the
    // rate from the recording itself.
    if (rec.size() > 100) {
        const double dur = static_cast<double>(rec.back().t - rec.front().t) * 1e-9;
        const double rate = dur > 0 ? static_cast<double>(rec.size() - 1) / dur : 0;
        if (rate > 0) r.addedLagMs = crossCorrelationLagSamples(rawX, procX, static_cast<int>(rate * 0.05)) / rate * 1000.0;
    }
    return r;
}

nlohmann::json toJson(const ProcessingCost& c) {
    return {{"samples", c.samples},        {"mean_ns", c.meanNs},       {"p50_ns", c.p50Ns},
            {"p99_ns", c.p99Ns},           {"p999_ns", c.p999Ns},       {"max_ns", c.maxNs},
            {"batch_mean_ns", c.batchMeanNs}, {"timer_resolution_ns", c.timerResolutionNs}};
}

nlohmann::json toJson(const SignalTestResult& r) {
    auto num = [](double v) -> nlohmann::json { return std::isfinite(v) ? nlohmann::json(v) : nlohmann::json(nullptr); };
    return {{"name", r.name},
            {"description", r.description},
            {"signal", {{"kind", toString(r.spec.kind)}, {"rate_hz", r.spec.rateHz}, {"amplitude", r.spec.amplitude},
                        {"freq_hz", r.spec.freqHz}, {"noise_std", r.spec.noiseStd}, {"seed", r.spec.seed}}},
            {"raw_t50_ms", num(r.rawT50Ms)},
            {"processed_t50_ms", num(r.procT50Ms)},
            {"raw_t90_ms", num(r.rawT90Ms)},
            {"processed_t90_ms", num(r.procT90Ms)},
            {"added_lag_ms", num(r.addedLagMs)},
            {"gain_ratio", num(r.gainRatio)},
            {"raw_rest_rms", num(r.rawRestRms)},
            {"processed_rest_rms", num(r.procRestRms)},
            {"processed_nonzero_fraction", r.procNonZeroFraction},
            {"raw_distinct_levels", r.rawDistinctLevels},
            {"processed_distinct_levels", r.procDistinctLevels},
            {"raw_jerk_rms", r.rawJerkRms},
            {"processed_jerk_rms", r.procJerkRms},
            {"processed_saturated_fraction", r.procSaturatedFraction}};
}

nlohmann::json toJson(const ComparisonReport& rep) {
    nlohmann::json tests = nlohmann::json::array();
    for (const auto& t : rep.tests) tests.push_back(toJson(t));
    return {{"kind", "objective_signal_comparison"},
            {"note", "Signal measurements only. These do not measure in-game aim performance or aim assist."},
            {"profile", rep.profileName},
            {"context", rep.context},
            {"rate_hz", rep.rateHz},
            {"tests", tests},
            {"processing_cost", toJson(rep.cost)}};
}

}  // namespace sa
