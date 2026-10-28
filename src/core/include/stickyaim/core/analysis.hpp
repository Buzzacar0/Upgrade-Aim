#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "stickyaim/core/pipeline.hpp"
#include "stickyaim/core/signals.hpp"

namespace sa {

// Objective, repeatable signal measurements comparing the raw stick signal
// with the processed output. Nothing here measures "aim" — only what the
// processing does to the signal.
struct SignalTestResult {
    std::string name;
    std::string description;
    SignalSpec spec;
    // Timing (ms). NaN when the metric does not apply to the test.
    double rawT50Ms = 0, procT50Ms = 0, addedLagMs = 0;
    double rawT90Ms = 0, procT90Ms = 0;
    // Amplitude: RMS(processed) / RMS(raw) along the test direction.
    double gainRatio = 0;
    // Noise: RMS of the output while the input is at rest (noise leaking through).
    double rawRestRms = 0, procRestRms = 0;
    double procNonZeroFraction = 0;  // fraction of samples where output != 0
    // Resolution: distinct int16 output levels produced over the test.
    int rawDistinctLevels = 0, procDistinctLevels = 0;
    // Smoothness: RMS of the second difference of the output (lower = smoother).
    double rawJerkRms = 0, procJerkRms = 0;
    double procSaturatedFraction = 0;  // |output| >= 0.999
};

struct ProcessingCost {
    std::size_t samples = 0;
    // Per-sample timings are quantized by the timer resolution (QPC is
    // typically 100 ns), so percentiles are coarse. batchMeanNs times the whole
    // run without per-sample timer calls and is the accurate average.
    double meanNs = 0, p50Ns = 0, p99Ns = 0, p999Ns = 0, maxNs = 0;
    double batchMeanNs = 0;
    double timerResolutionNs = 0;
};

struct ComparisonReport {
    std::string profileName;
    std::string context;
    double rateHz = 0;
    std::vector<SignalTestResult> tests;
    ProcessingCost cost;
};

// Runs the built-in synthetic suite (rest+noise, drift, step, sine tracking,
// micro-adjust, flick) through the profile with the given context forced.
ComparisonReport runSyntheticComparison(const CompiledProfile& profile, ContextId context, double rateHz = 1000.0);

// Runs a single signal through the profile and computes metrics.
SignalTestResult measureSignal(const CompiledProfile& profile, ContextId context, const std::string& name,
                               const std::string& description, const SignalSpec& spec);

// Measures the cost of Processor::process() on this machine.
ProcessingCost benchmarkProcessing(const CompiledProfile& profile, std::size_t samples = 200000);

// Metrics for a recording captured from a real device, replayed through the profile.
SignalTestResult analyzeRecording(const CompiledProfile& profile, const std::vector<RawSample>& recording,
                                  const std::string& name);

nlohmann::json toJson(const SignalTestResult& r);
nlohmann::json toJson(const ComparisonReport& r);
nlohmann::json toJson(const ProcessingCost& c);

}  // namespace sa
