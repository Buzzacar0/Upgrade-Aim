#include <gtest/gtest.h>

#include <cmath>

#include "stickyaim/core/deadzone.hpp"

using namespace sa;

TEST(Deadzone, RadialInnerReadsZero) {
    const DeadzoneConfig dz{DeadzoneType::Radial, 0.1f, 0.9f};
    EXPECT_FLOAT_EQ(length(applyDeadzone({0.07f, 0.07f}, dz)), 0.0f);  // |v| = 0.099
    EXPECT_FLOAT_EQ(length(applyDeadzone({0.1f, 0.0f}, dz)), 0.0f);    // exactly on threshold
    EXPECT_GT(length(applyDeadzone({0.0f, 0.101f}, dz)), 0.0f);
}

TEST(Deadzone, RadialLinearBetweenThresholdsAndPreservesDirection) {
    const DeadzoneConfig dz{DeadzoneType::Radial, 0.1f, 0.9f};
    const Vec2 in{0.3f, 0.4f};  // |v| = 0.5 -> (0.5 - 0.1) / 0.8 = 0.5
    const Vec2 out = applyDeadzone(in, dz);
    EXPECT_NEAR(length(out), 0.5f, 1e-6f);
    EXPECT_NEAR(std::atan2(out.y, out.x), std::atan2(in.y, in.x), 1e-6f);
}

TEST(Deadzone, OuterThresholdSaturates) {
    const DeadzoneConfig dz{DeadzoneType::Radial, 0.05f, 0.9f};
    EXPECT_NEAR(length(applyDeadzone({0.9f, 0.0f}, dz)), 1.0f, 1e-6f);
    EXPECT_NEAR(length(applyDeadzone({1.0f, 1.0f}, dz)), 1.0f, 1e-6f);  // square-gate corner
}

TEST(Deadzone, AxialAxesIndependent) {
    const DeadzoneConfig dz{DeadzoneType::Axial, 0.1f, 1.0f};
    const Vec2 out = applyDeadzone({0.05f, 0.55f}, dz);
    EXPECT_FLOAT_EQ(out.x, 0.0f);  // X within its own deadzone
    EXPECT_NEAR(out.y, 0.5f, 1e-6f);
    const Vec2 neg = applyDeadzone({-0.55f, -0.05f}, dz);
    EXPECT_NEAR(neg.x, -0.5f, 1e-6f);
    EXPECT_FLOAT_EQ(neg.y, 0.0f);
}

TEST(Deadzone, NoneOnlySaturates) {
    const DeadzoneConfig dz{DeadzoneType::None, 0.2f, 0.8f};
    EXPECT_NEAR(applyDeadzone({0.01f, 0.0f}, dz).x, 0.0125f, 1e-6f);
    EXPECT_NEAR(length(applyDeadzone({0.0f, 0.9f}, dz)), 1.0f, 1e-6f);
}

TEST(Deadzone, ContinuousAtInnerEdge) {
    const DeadzoneConfig dz{DeadzoneType::Radial, 0.08f, 0.95f};
    float prev = 0.0f;
    for (int i = 0; i <= 10000; ++i) {
        const float m = length(applyDeadzone({i / 10000.0f, 0.0f}, dz));
        ASSERT_GE(m, prev);
        ASSERT_LT(m - prev, 0.01f) << "jump at " << i;  // no discontinuity
        prev = m;
    }
}

TEST(Deadzone, SymmetricInAllQuadrants) {
    const DeadzoneConfig dz{DeadzoneType::Radial, 0.1f, 0.95f};
    const Vec2 a = applyDeadzone({0.4f, 0.2f}, dz);
    const Vec2 b = applyDeadzone({-0.4f, -0.2f}, dz);
    const Vec2 c = applyDeadzone({0.2f, 0.4f}, dz);
    EXPECT_FLOAT_EQ(a.x, -b.x);
    EXPECT_FLOAT_EQ(a.y, -b.y);
    EXPECT_FLOAT_EQ(a.x, c.y);
    EXPECT_FLOAT_EQ(a.y, c.x);
}

TEST(Deadzone, RescaleThresholdEdges) {
    EXPECT_FLOAT_EQ(rescaleThreshold(0.0f, 0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(rescaleThreshold(1.0f, 0.0f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(rescaleThreshold(2.0f, 0.0f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(rescaleThreshold(std::nanf(""), 0.1f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(rescaleThreshold(0.5f, 0.5f, 0.5f), 0.0f);  // degenerate span, at threshold
    EXPECT_FLOAT_EQ(rescaleThreshold(0.6f, 0.5f, 0.5f), 1.0f);
}

TEST(Deadzone, Validation) {
    EXPECT_TRUE(validate(DeadzoneConfig{DeadzoneType::Radial, 0.05f, 0.98f}).empty());
    EXPECT_FALSE(validate(DeadzoneConfig{DeadzoneType::Radial, -0.1f, 0.98f}).empty());
    EXPECT_FALSE(validate(DeadzoneConfig{DeadzoneType::Radial, 0.5f, 0.52f}).empty());
    EXPECT_FALSE(validate(DeadzoneConfig{DeadzoneType::Radial, 0.1f, 1.2f}).empty());
}
