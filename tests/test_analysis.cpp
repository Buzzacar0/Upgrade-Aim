#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "stickyaim/core/analysis.hpp"
#include "stickyaim/core/clock.hpp"

using namespace sa;

namespace {

std::unique_ptr<CompiledProfile> make(const Profile& p) {
    auto cp = CompiledProfile::compile(p, {});
    EXPECT_TRUE(cp);
    return cp;
}

const SignalTestResult& find(const ComparisonReport& r, const std::string& name) {
    for (const auto& t : r.tests)
        if (t.name == name) return t;
    throw std::runtime_error("missing test " + name);
}

}  // namespace

TEST(Analysis, StaticProcessingAddsNoLatency) {
    Profile p;
    p.hip.radial.curve = {CurveType::Power, 2.0f};
    p.ads = p.hip;
    auto cp = make(p);
    const auto rep = runSyntheticComparison(*cp, ContextId::Hip, 1000.0);
    EXPECT_NEAR(find(rep, "step_50").addedLagMs, 0.0, 1e-9);
    EXPECT_NEAR(find(rep, "step_100").addedLagMs, 0.0, 1e-9);
    EXPECT_NEAR(find(rep, "tracking_1hz").addedLagMs, 0.0, 1e-9);
}

TEST(Analysis, SmoothingLagIsMeasured) {
    Profile p;
    p.hip.smoothing.type = SmoothingType::Ema;
    p.hip.smoothing.emaTauMs = 10.0f;
    p.hip.deadzone.inner = 0.0f;  // keep the 50% point of output equal to 50% of input
    p.ads = p.hip;
    auto cp = make(p);
    const auto rep = runSyntheticComparison(*cp, ContextId::Hip, 1000.0);
    const auto& step = find(rep, "step_50");
    // EMA reaches 50% after tau * ln 2 = 6.9 ms (+/- one sample).
    EXPECT_NEAR(step.addedLagMs, 10.0 * std::log(2.0), 1.5);
    EXPECT_GT(find(rep, "tracking_1hz").addedLagMs, 5.0);
    EXPECT_LT(find(rep, "micro_adjust").procJerkRms, find(rep, "micro_adjust").rawJerkRms);
}

TEST(Analysis, FlickBypassRemovesSmoothingLagOnFlicks) {
    Profile p;
    p.hip.smoothing.type = SmoothingType::Ema;
    p.hip.smoothing.emaTauMs = 10.0f;
    p.hip.smoothing.flickBypassSpeed = 50.0f;
    p.ads = p.hip;
    auto cp = make(p);
    const auto rep = runSyntheticComparison(*cp, ContextId::Hip, 1000.0);
    EXPECT_NEAR(find(rep, "flick_left").addedLagMs, 0.0, 1e-9);
}

TEST(Analysis, DeadzoneSuppressesRestNoise) {
    Profile p;
    p.hip.deadzone.inner = 0.06f;
    p.ads = p.hip;
    auto cp = make(p);
    const auto rep = runSyntheticComparison(*cp, ContextId::Hip, 1000.0);
    const auto& rest = find(rep, "rest_noise");
    EXPECT_GT(rest.rawRestRms, 0.01);
    EXPECT_EQ(rest.procRestRms, 0.0);
    EXPECT_EQ(rest.procNonZeroFraction, 0.0);
}

TEST(Analysis, FineZoneIncreasesMicroAdjustResolution) {
    Profile coarse;
    coarse.hip.radial.curve = {CurveType::Linear};
    coarse.ads = coarse.hip;
    Profile fine = coarse;
    fine.hip.radial.curve = {CurveType::Power, 2.0f};
    fine.ads = fine.hip;
    auto a = make(coarse);
    auto b = make(fine);
    const auto ra = runSyntheticComparison(*a, ContextId::Hip, 1000.0);
    const auto rb = runSyntheticComparison(*b, ContextId::Hip, 1000.0);
    // A convex curve compresses small inputs: lower gain in the micro range.
    EXPECT_LT(find(rb, "micro_adjust").gainRatio, find(ra, "micro_adjust").gainRatio);
}

TEST(Analysis, ReportSerializes) {
    Profile p;
    auto cp = make(p);
    const auto rep = runSyntheticComparison(*cp, ContextId::Ads, 500.0);
    const auto j = toJson(rep);
    EXPECT_EQ(j["kind"], "objective_signal_comparison");
    EXPECT_EQ(j["tests"].size(), rep.tests.size());
    EXPECT_TRUE(j["tests"][0]["raw_t50_ms"].is_null());  // rest test has no step timing
}

// Processing cost must stay small even while other threads saturate the CPU.
TEST(Latency, ProcessingCostUnderLoad) {
    Profile p;
    p.hip.radial.curve = {CurveType::Custom, 1.0f, 0.0f, {{0.2f, 0.1f}, {0.5f, 0.4f}, {0.8f, 0.85f}}};
    p.hip.radial.fine = {true, 0.0f, 0.2f, 0.6f, 0.05f};
    p.hip.smoothing.type = SmoothingType::OneEuro;
    p.ads = p.hip;
    p.contextTransitionMs = 10.0f;
    auto cp = make(p);

    std::atomic<bool> stop{false};
    std::vector<std::thread> load;
    const unsigned n = std::max(2u, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < n; ++i)
        load.emplace_back([&] {
            volatile double x = 1.0;
            while (!stop.load(std::memory_order_relaxed)) x = std::sqrt(x + 1.0);
        });
    const ProcessingCost cost = benchmarkProcessing(*cp, 200000);
    stop = true;
    for (auto& t : load) t.join();

    RecordProperty("mean_ns", std::to_string(cost.meanNs));
    RecordProperty("p99_ns", std::to_string(cost.p99Ns));
    std::printf("[ processing cost under load ] batch mean %.1f ns | per-sample p50 %.0f, p99 %.0f, p99.9 %.0f, max %.0f ns"
                " (timer resolution %.0f ns)\n",
                cost.batchMeanNs, cost.p50Ns, cost.p99Ns, cost.p999Ns, cost.maxNs, cost.timerResolutionNs);
    EXPECT_EQ(cost.samples, 200000u);
    EXPECT_GT(cost.batchMeanNs, 0.0);
    EXPECT_LT(cost.batchMeanNs, 5000.0);
    // Generous bounds: a 8 kHz source leaves 125 us per sample. The median
    // must be far below that even on a loaded machine.
    EXPECT_LT(cost.p50Ns, 5000.0);
    EXPECT_LT(cost.p99Ns, 50000.0);
}
