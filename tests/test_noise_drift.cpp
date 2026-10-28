#include <gtest/gtest.h>

#include <cmath>

#include "stickyaim/core/calibration.hpp"
#include "stickyaim/core/pipeline.hpp"
#include "stickyaim/core/signals.hpp"

using namespace sa;

namespace {
Vec2 rightStick(const RawSample& s) { return {normalizeAxis(s.pad.rx), normalizeAxis(s.pad.ry)}; }
}

TEST(RestAnalysis, MeasuresOffsetAndNoise) {
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::Rest;
    spec.durationSec = 3.0;
    spec.noiseStd = 0.005f;
    spec.offset = {0.03f, -0.02f};
    RestAnalyzer ra;
    for (const auto& s : generate(spec)) ra.add(rightStick(s));
    const RestReport r = ra.report();
    EXPECT_EQ(r.samples, 3000u);
    EXPECT_NEAR(r.mean.x, 0.03f, 0.001f);
    EXPECT_NEAR(r.mean.y, -0.02f, 0.001f);
    EXPECT_NEAR(r.stddev.x, 0.005f, 0.0005f);
    EXPECT_NEAR(r.diffRms, 0.005f * std::sqrt(2.0f) * std::sqrt(2.0f), 0.002f);  // 2D diff of iid noise
    EXPECT_TRUE(r.noisy);
    EXPECT_FALSE(r.driftSuspected);
    EXPECT_GT(r.recommendedInnerUncalibrated, r.recommendedInnerCalibrated);
}

TEST(RestAnalysis, RecommendedDeadzoneSuppressesAllRestNoise) {
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::Rest;
    spec.durationSec = 2.0;
    spec.noiseStd = 0.006f;
    spec.offset = {0.04f, 0.01f};
    const auto samples = generate(spec);
    RestAnalyzer ra;
    for (const auto& s : samples) ra.add(rightStick(s));
    const RestReport r = ra.report();

    // Using the uncalibrated recommendation: every rest sample reads zero.
    Profile p;
    p.hip.deadzone.inner = r.recommendedInnerUncalibrated;
    p.ads = p.hip;
    auto cp = CompiledProfile::compile(p, {});
    ASSERT_TRUE(cp);
    Processor proc;
    proc.setProfile(cp.get());
    for (const auto& s : samples) ASSERT_FLOAT_EQ(length(proc.process(s).right), 0.0f);

    // Using the calibrated recommendation together with the measured center.
    Profile pc;
    pc.hip.deadzone.inner = r.recommendedInnerCalibrated;
    pc.ads = pc.hip;
    DeviceCalibration cal;
    cal.right.centerX = r.mean.x;
    cal.right.centerY = r.mean.y;
    auto cpc = CompiledProfile::compile(pc, cal);
    ASSERT_TRUE(cpc);
    Processor proc2;
    proc2.setProfile(cpc.get());
    for (const auto& s : samples) ASSERT_FLOAT_EQ(length(proc2.process(s).right), 0.0f);
}

TEST(RestAnalysis, FlagsDrift) {
    RestAnalyzer ra;
    for (int i = 0; i < 500; ++i) ra.add({0.09f, 0.0f});
    EXPECT_TRUE(ra.report().driftSuspected);
}

TEST(RangeAnalysis, CircleCoversAllSectors) {
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::Circle;
    spec.durationSec = 2.0;
    spec.amplitude = 0.97f;
    RangeAnalyzer ra;
    for (const auto& s : generate(spec)) ra.add(rightStick(s));
    const RangeReport r = ra.report();
    EXPECT_EQ(r.sectorsCovered, RangeReport::kSectors);
    EXPECT_NEAR(r.minSectorMax, 0.97f, 0.01f);
    EXPECT_LT(r.circularityError, 0.02f);
    EXPECT_NEAR(r.recommendedOuter, 0.97f * 0.98f, 0.01f);
    const StickCalibration c = ra.toCalibration({0, 0});
    EXPECT_NEAR(c.maxPosX, 0.97f, 0.01f);
    EXPECT_NEAR(c.maxNegY, 0.97f, 0.01f);
}

TEST(RangeAnalysis, PartialSweepIsReported) {
    RangeAnalyzer ra;
    for (int i = 0; i < 100; ++i) ra.add({0.9f, 0.01f * i});  // only a narrow arc
    EXPECT_LT(ra.report().sectorsCovered, 10);
}

TEST(DriftMonitor, TracksSlowRestOffset) {
    DriftMonitor dm(0.2f);
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::Rest;
    spec.durationSec = 6.0;
    spec.noiseStd = 0.001f;
    spec.offset = {0.05f, 0.0f};
    for (const auto& s : generate(spec)) dm.add(rightStick(s), s.t);
    ASSERT_TRUE(dm.hasEstimate());
    EXPECT_NEAR(dm.restEstimate().x, 0.05f, 0.005f);
    EXPECT_NEAR(dm.deadzoneUsage(0.1f), 0.5f, 0.06f);
}

TEST(DriftMonitor, IgnoresActiveMovement) {
    DriftMonitor dm(0.2f);
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::Sine;
    spec.durationSec = 5.0;
    spec.amplitude = 0.15f;  // within the rest radius, but moving
    spec.freqHz = 2.0;
    for (const auto& s : generate(spec)) dm.add(rightStick(s), s.t);
    EXPECT_FALSE(dm.hasEstimate());
}
