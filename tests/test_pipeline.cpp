#include <gtest/gtest.h>

#include <cmath>

#include "stickyaim/core/pipeline.hpp"
#include "stickyaim/core/signals.hpp"

using namespace sa;

namespace {

RawSample sample(std::int16_t rx, std::int16_t ry, TimestampNs t = 0, std::uint8_t lt = 0) {
    RawSample s;
    s.t = t;
    s.pad.rx = rx;
    s.pad.ry = ry;
    s.pad.leftTrigger = lt;
    return s;
}

std::unique_ptr<CompiledProfile> compile(const Profile& p) {
    std::vector<std::string> errs;
    auto cp = CompiledProfile::compile(p, DeviceCalibration{}, &errs);
    EXPECT_TRUE(cp) << (errs.empty() ? "" : errs.front());
    return cp;
}

Profile curvedProfile() {
    Profile p;
    p.hip.radial.curve = {CurveType::Power, 1.6f};
    p.hip.deadzone = {DeadzoneType::Radial, 0.06f, 0.97f};
    p.ads = p.hip;
    p.ads.radial.curve = {CurveType::Power, 2.2f};
    return p;
}

}  // namespace

TEST(Normalization, ExtremesAndSymmetry) {
    EXPECT_FLOAT_EQ(normalizeAxis(32767), 1.0f);
    EXPECT_FLOAT_EQ(normalizeAxis(-32767), -1.0f);
    EXPECT_FLOAT_EQ(normalizeAxis(-32768), -1.0f);  // clamped, not -1.00003
    EXPECT_FLOAT_EQ(normalizeAxis(0), 0.0f);
    for (int v = -32767; v <= 32767; v += 97)
        EXPECT_FLOAT_EQ(normalizeAxis(static_cast<std::int16_t>(v)), -normalizeAxis(static_cast<std::int16_t>(-v)));
    EXPECT_EQ(denormalizeAxis(1.5f), 32767);
    EXPECT_EQ(denormalizeAxis(-1.5f), -32767);
    EXPECT_EQ(denormalizeAxis(std::nanf("")), 0);
    EXPECT_EQ(denormalizeTrigger(std::nanf("")), 0);
    for (int v = -32767; v <= 32767; v += 13)
        EXPECT_EQ(denormalizeAxis(normalizeAxis(static_cast<std::int16_t>(v))), v);  // lossless round trip
    EXPECT_EQ(denormalizeTrigger(normalizeTrigger(200)), 200);
}

TEST(Pipeline, NullProfileIsPassThrough) {
    Processor proc;
    const RawSample s = sample(1234, -4321);
    const RawGamepad out = denormalize(proc.process(s));
    EXPECT_EQ(out.rx, 1234);
    EXPECT_EQ(out.ry, -4321);
}

TEST(Pipeline, DefaultProfileLinearOutsideDeadzone) {
    Profile p;
    p.hip.deadzone = {DeadzoneType::Radial, 0.0f, 1.0f};
    p.ads = p.hip;
    auto cp = compile(p);
    Processor proc;
    proc.setProfile(cp.get());
    for (int v : {0, 100, 5000, 16000, 32767}) {
        const auto out = proc.process(sample(static_cast<std::int16_t>(v), 0));
        EXPECT_NEAR(out.right.x, normalizeAxis(static_cast<std::int16_t>(v)), 1e-4f);
    }
}

TEST(Pipeline, AxisSymmetry) {
    auto cp = compile(curvedProfile());
    Processor proc;
    proc.setProfile(cp.get());
    for (int v = 0; v <= 32767; v += 1111) {
        const auto px = proc.process(sample(static_cast<std::int16_t>(v), 0));
        const auto nx = proc.process(sample(static_cast<std::int16_t>(-v), 0));
        const auto py = proc.process(sample(0, static_cast<std::int16_t>(v)));
        const auto ny = proc.process(sample(0, static_cast<std::int16_t>(-v)));
        EXPECT_FLOAT_EQ(px.right.x, -nx.right.x) << v;
        EXPECT_FLOAT_EQ(py.right.y, -ny.right.y) << v;
        EXPECT_FLOAT_EQ(px.right.x, py.right.y) << v;  // X and Y identical when configured identically
    }
}

TEST(Pipeline, PerAxisModeAppliesSeparateCurves) {
    Profile p;
    p.hip.curveMode = CurveMode::PerAxis;
    p.hip.deadzone = {DeadzoneType::Axial, 0.0f, 1.0f};
    p.hip.x.curve = {CurveType::Linear};
    p.hip.y.curve = {CurveType::Power, 2.0f};
    p.ads = p.hip;
    auto cp = compile(p);
    Processor proc;
    proc.setProfile(cp.get());
    const auto out = proc.process(sample(16384, 16384));
    EXPECT_NEAR(out.right.x, 0.5f, 1e-3f);
    EXPECT_NEAR(out.right.y, 0.25f, 1e-3f);
}

TEST(Pipeline, SensitivityAndCircularSaturation) {
    Profile p;
    p.hip.deadzone = {DeadzoneType::Radial, 0.0f, 1.0f};
    p.hip.sensX = 2.0f;
    p.hip.sensY = 0.5f;
    p.ads = p.hip;
    auto cp = compile(p);
    Processor proc;
    proc.setProfile(cp.get());
    const auto a = proc.process(sample(8192, 8192));
    EXPECT_NEAR(a.right.x, 0.5f, 1e-3f);
    EXPECT_NEAR(a.right.y, 0.125f, 1e-3f);
    const auto b = proc.process(sample(32767, 32767));
    EXPECT_LE(length(b.right), 1.0f + 1e-5f);
    EXPECT_LE(std::fabs(b.right.x), 1.0f);
}

TEST(Pipeline, OutputAlwaysWithinRange) {
    auto cp = compile(curvedProfile());
    Processor proc;
    proc.setProfile(cp.get());
    for (int x = -32768; x <= 32767; x += 4099)
        for (int y = -32768; y <= 32767; y += 4099) {
            const auto o = proc.process(sample(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)));
            ASSERT_LE(std::fabs(o.right.x), 1.0f);
            ASSERT_LE(std::fabs(o.right.y), 1.0f);
            ASSERT_LE(length(o.right), 1.0f + 1e-5f);
        }
}

TEST(Pipeline, AntiDeadzoneLiftsSmallOutputs) {
    Profile p;
    p.hip.deadzone = {DeadzoneType::Radial, 0.05f, 1.0f};
    p.hip.antiDeadzone = 0.1f;
    p.ads = p.hip;
    auto cp = compile(p);
    Processor proc;
    proc.setProfile(cp.get());
    EXPECT_FLOAT_EQ(length(proc.process(sample(1000, 0)).right), 0.0f);  // inside deadzone stays 0
    EXPECT_GE(length(proc.process(sample(2000, 0)).right), 0.1f);        // just outside jumps to >= 0.1
    EXPECT_NEAR(length(proc.process(sample(32767, 0)).right), 1.0f, 1e-5f);
}

TEST(Pipeline, AdsContextHysteresis) {
    auto cp = compile(curvedProfile());
    Processor proc;
    proc.setProfile(cp.get());
    const std::uint8_t enter = static_cast<std::uint8_t>(0.15f * 255 + 1);
    const std::uint8_t between = static_cast<std::uint8_t>(0.11f * 255);
    const std::uint8_t exit = static_cast<std::uint8_t>(0.05f * 255);
    proc.process(sample(0, 0, 0, between));
    EXPECT_EQ(proc.activeContext(), ContextId::Hip);
    proc.process(sample(0, 0, 1, enter));
    EXPECT_EQ(proc.activeContext(), ContextId::Ads);
    proc.process(sample(0, 0, 2, between));
    EXPECT_EQ(proc.activeContext(), ContextId::Ads);  // still above exit threshold
    proc.process(sample(0, 0, 3, exit));
    EXPECT_EQ(proc.activeContext(), ContextId::Hip);
}

TEST(Pipeline, ContextUsesItsOwnCurve) {
    auto cp = compile(curvedProfile());
    Processor proc;
    proc.setProfile(cp.get());
    const float hip = proc.process(sample(20000, 0, 0, 0)).right.x;
    const float ads = proc.process(sample(20000, 0, 1, 255)).right.x;
    EXPECT_GT(hip, ads);  // ADS curve has a higher exponent
}

TEST(Pipeline, ContextTransitionIsGradualAndBounded) {
    Profile p = curvedProfile();
    p.contextTransitionMs = 50.0f;
    auto cp = compile(p);
    Processor proc;
    proc.setProfile(cp.get());
    ProcessTrace tr;
    proc.process(sample(20000, 0, 0, 0), &tr);
    EXPECT_FLOAT_EQ(tr.adsBlend, 0.0f);
    proc.process(sample(20000, 0, 25'000'000, 255), &tr);  // 25 ms later
    EXPECT_NEAR(tr.adsBlend, 0.5f, 1e-4f);
    EXPECT_TRUE(proc.needsTick());
    proc.process(sample(20000, 0, 80'000'000, 255), &tr);
    EXPECT_FLOAT_EQ(tr.adsBlend, 1.0f);
}

TEST(Pipeline, DeterministicForIdenticalInput) {
    Profile p = curvedProfile();
    p.hip.smoothing.type = SmoothingType::OneEuro;
    p.contextTransitionMs = 20.0f;
    auto cp = compile(p);
    SignalSpec spec;
    spec.kind = SignalSpec::Kind::MicroAdjust;
    spec.durationSec = 0.5;
    spec.noiseStd = 0.01f;
    spec.timingJitterUs = 100;
    const auto samples = generate(spec);
    Processor a, b;
    a.setProfile(cp.get());
    b.setProfile(cp.get());
    for (const auto& s : samples) {
        const auto oa = a.process(s);
        const auto ob = b.process(s);
        ASSERT_EQ(oa.right.x, ob.right.x);
        ASSERT_EQ(oa.right.y, ob.right.y);
    }
}

TEST(Pipeline, CalibrationRemovesCenterOffset) {
    Profile p;
    p.hip.deadzone = {DeadzoneType::Radial, 0.03f, 1.0f};
    p.ads = p.hip;
    DeviceCalibration cal;
    cal.right.centerX = 0.08f;  // drifted stick resting at +8%
    auto cp = CompiledProfile::compile(p, cal);
    ASSERT_TRUE(cp);
    Processor proc;
    proc.setProfile(cp.get());
    EXPECT_FLOAT_EQ(length(proc.process(sample(denormalizeAxis(0.08f), 0)).right), 0.0f);
    // Both extremes still reach full output after recentering.
    EXPECT_NEAR(proc.process(sample(32767, 0)).right.x, 1.0f, 1e-4f);
    EXPECT_NEAR(proc.process(sample(-32767, 0)).right.x, -1.0f, 1e-4f);
}

TEST(Pipeline, ButtonsAndTriggersPassThrough) {
    Profile p;
    p.leftTrigger = {0.1f, 0.9f};
    auto cp = compile(p);
    Processor proc;
    proc.setProfile(cp.get());
    RawSample s = sample(0, 0);
    s.pad.buttons = Button::A | Button::RightShoulder;
    s.pad.leftTrigger = 20;    // below 10%
    s.pad.rightTrigger = 128;
    const auto out = proc.process(s);
    EXPECT_EQ(out.buttons, s.pad.buttons);
    EXPECT_FLOAT_EQ(out.lt, 0.0f);
    EXPECT_NEAR(out.rt, 128.0f / 255.0f, 1e-6f);
}

TEST(Pipeline, InvalidProfileRefusesToCompile) {
    Profile p;
    p.hip.deadzone.inner = 0.95f;
    std::vector<std::string> errs;
    EXPECT_EQ(CompiledProfile::compile(p, {}, &errs), nullptr);
    EXPECT_FALSE(errs.empty());
}
