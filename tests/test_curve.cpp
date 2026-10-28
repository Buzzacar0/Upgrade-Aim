#include <gtest/gtest.h>

#include <cmath>

#include "stickyaim/core/curve.hpp"
#include "stickyaim/core/response.hpp"

using namespace sa;

namespace {

std::vector<CurveSpec> representativeCurves() {
    std::vector<CurveSpec> out;
    out.push_back({CurveType::Linear});
    for (float e : {0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 10.0f}) out.push_back({CurveType::Power, e});
    for (float k : {-12.0f, -3.0f, -0.5f, 0.0f, 0.00001f, 0.5f, 3.0f, 12.0f}) out.push_back({CurveType::Exponential, 1.0f, k});
    CurveSpec custom{CurveType::Custom};
    custom.points = {{0.1f, 0.02f}, {0.3f, 0.15f}, {0.6f, 0.5f}, {0.9f, 0.95f}};
    out.push_back(custom);
    CurveSpec flat{CurveType::Custom};
    flat.points = {{0.2f, 0.3f}, {0.5f, 0.3f}, {0.8f, 0.3f}};  // plateau must stay flat, not overshoot
    out.push_back(flat);
    CurveSpec steep{CurveType::Custom};
    steep.points = {{0.05f, 0.6f}, {0.1f, 0.62f}};
    out.push_back(steep);
    return out;
}

}  // namespace

TEST(Curve, BoundaryConditionsHoldForAllTypes) {
    for (const auto& spec : representativeCurves()) {
        const Curve c = Curve::fromSpec(spec);
        EXPECT_NEAR(c(0.0f), 0.0f, 1e-6f) << toString(spec.type);
        EXPECT_NEAR(c(1.0f), 1.0f, 1e-5f) << toString(spec.type);
        // Out-of-domain inputs clamp.
        EXPECT_NEAR(c(-0.5f), 0.0f, 1e-6f);
        EXPECT_NEAR(c(1.5f), 1.0f, 1e-5f);
        EXPECT_NEAR(c(std::nanf("")), 0.0f, 1e-6f);
    }
}

TEST(Curve, MonotoneNonDecreasingForAllTypes) {
    for (const auto& spec : representativeCurves()) {
        const Curve c = Curve::fromSpec(spec);
        float prev = c(0.0f);
        for (int i = 1; i <= 10000; ++i) {
            const float v = c(static_cast<float>(i) / 10000.0f);
            ASSERT_GE(v, prev - 1e-7f) << toString(spec.type) << " at t=" << i / 10000.0;
            ASSERT_GE(v, 0.0f);
            ASSERT_LE(v, 1.0f + 1e-6f);
            prev = v;
        }
    }
}

TEST(Curve, PowerAndExponentialShape) {
    const Curve p2 = Curve::fromSpec({CurveType::Power, 2.0f});
    EXPECT_NEAR(p2(0.5f), 0.25f, 1e-6f);
    const Curve e = Curve::fromSpec({CurveType::Exponential, 1.0f, 3.0f});
    EXPECT_LT(e(0.5f), 0.5f);  // positive k: slow start
    const Curve en = Curve::fromSpec({CurveType::Exponential, 1.0f, -3.0f});
    EXPECT_GT(en(0.5f), 0.5f);  // negative k: fast start
    const Curve nearZero = Curve::fromSpec({CurveType::Exponential, 1.0f, 1e-6f});
    EXPECT_NEAR(nearZero(0.37f), 0.37f, 1e-5f);
}

TEST(Curve, CustomPassesThroughControlPoints) {
    CurveSpec s{CurveType::Custom};
    s.points = {{0.25f, 0.1f}, {0.5f, 0.4f}, {0.75f, 0.8f}};
    const Curve c = Curve::fromSpec(s);
    for (const auto& p : s.points) EXPECT_NEAR(c(p.x), p.y, 1e-5f);
}

TEST(Curve, SanitizeRepairsBadCustomPoints) {
    CurveSpec s{CurveType::Custom};
    s.points = {{0.7f, 0.3f}, {0.2f, 0.5f}, {1.4f, 2.0f}, {0.0f, 0.4f}, {0.5f, std::nanf("")}};
    EXPECT_FALSE(validate(s).empty());
    const CurveSpec clean = sanitize(s);
    EXPECT_TRUE(validate(clean).empty());
    ASSERT_EQ(clean.points.size(), 2u);  // (0.2,0.5) and (0.7, max(0.3,0.5))
    EXPECT_FLOAT_EQ(clean.points[0].x, 0.2f);
    EXPECT_FLOAT_EQ(clean.points[1].y, 0.5f);
}

TEST(Curve, ParameterClamping) {
    EXPECT_FALSE(validate(CurveSpec{CurveType::Power, 0.0f}).empty());
    EXPECT_FALSE(validate(CurveSpec{CurveType::Exponential, 1.0f, 50.0f}).empty());
    const CurveSpec s = sanitize(CurveSpec{CurveType::Power, -4.0f});
    EXPECT_FLOAT_EQ(s.exponent, 0.1f);
}

TEST(Curve, TypeNamesRoundTrip) {
    for (auto t : {CurveType::Linear, CurveType::Power, CurveType::Exponential, CurveType::Custom}) {
        CurveType back{};
        ASSERT_TRUE(curveTypeFromString(toString(t), back));
        EXPECT_EQ(back, t);
    }
    CurveType dummy{};
    EXPECT_FALSE(curveTypeFromString("bogus", dummy));
}

// ----------------------------------------------------------------- ResponseLut

TEST(ResponseLut, IdentityByDefault) {
    const ResponseLut lut;
    for (float t : {0.0f, 0.1f, 0.5f, 0.999f, 1.0f}) EXPECT_NEAR(lut.eval(t), t, 1e-6f);
}

TEST(ResponseLut, MatchesCurveWithoutZones) {
    ResponseShape s;
    s.curve = {CurveType::Power, 1.8f};
    const ResponseLut lut = ResponseLut::build(s);
    const Curve c = Curve::fromSpec(s.curve);
    for (int i = 0; i <= 1000; ++i) {
        const float t = i / 1000.0f;
        EXPECT_NEAR(lut.eval(t), c(t), 2e-4f) << t;
    }
}

TEST(ResponseLut, MonotoneAndBoundedWithZones) {
    for (float fineGain : {0.05f, 0.4f, 1.0f, 2.5f}) {
        for (float fastGain : {0.3f, 1.0f, 3.0f, 20.0f}) {
            ResponseShape s;
            s.curve = {CurveType::Exponential, 1.0f, 2.0f};
            s.fine = {true, 0.0f, 0.3f, fineGain, 0.1f};
            s.fast = {true, 0.7f, 1.0f, fastGain, 0.1f};
            const ResponseLut lut = ResponseLut::build(s);
            EXPECT_EQ(lut.eval(0.0f), 0.0f);
            EXPECT_EQ(lut.eval(1.0f), 1.0f);
            float prev = 0.0f;
            for (int i = 0; i <= 20000; ++i) {
                const float v = lut.eval(i / 20000.0f);
                ASSERT_GE(v, prev) << "fine=" << fineGain << " fast=" << fastGain;
                prev = v;
            }
        }
    }
}

TEST(ResponseLut, FineZoneReducesSlopeNearCenter) {
    ResponseShape base;
    ResponseShape fine = base;
    fine.fine = {true, 0.0f, 0.25f, 0.5f, 0.05f};
    const ResponseLut a = ResponseLut::build(base);
    const ResponseLut b = ResponseLut::build(fine);
    // Inside the zone the response is lower (more resolution for micro-corrections)...
    EXPECT_LT(b.eval(0.1f), a.eval(0.1f) * 0.75f);
    // ...and full deflection is still reachable.
    EXPECT_FLOAT_EQ(b.eval(1.0f), 1.0f);
}

TEST(ResponseLut, ZoneMultiplierIsContinuous) {
    const SlopeZone z{true, 0.3f, 0.6f, 3.0f, 0.1f};
    float prev = zoneMultiplier(z, 0.0f);
    for (int i = 1; i <= 1000; ++i) {
        const float v = zoneMultiplier(z, i / 1000.0f);
        EXPECT_LT(std::fabs(v - prev), 0.1f);
        prev = v;
    }
    EXPECT_FLOAT_EQ(zoneMultiplier(z, 0.45f), 3.0f);
    EXPECT_FLOAT_EQ(zoneMultiplier(z, 0.1f), 1.0f);
    EXPECT_FLOAT_EQ(zoneMultiplier(SlopeZone{}, 0.1f), 1.0f);  // disabled
}
