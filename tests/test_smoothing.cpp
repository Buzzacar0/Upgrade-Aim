#include <gtest/gtest.h>

#include <cmath>

#include "stickyaim/core/smoothing.hpp"

using namespace sa;

namespace {
constexpr TimestampNs kMs = 1'000'000;
}

TEST(Smoothing, NoneIsIdentity) {
    Smoother2D s;
    SmoothingConfig cfg;  // None
    for (int i = 0; i < 100; ++i) {
        const Vec2 v{std::sin(i * 0.1f), std::cos(i * 0.3f)};
        const Vec2 o = s.apply(v, i * kMs, cfg);
        EXPECT_EQ(o.x, v.x);
        EXPECT_EQ(o.y, v.y);
    }
    EXPECT_FALSE(s.settling());
}

TEST(Smoothing, EmaStepResponseMatchesTimeConstant) {
    Smoother2D s;
    SmoothingConfig cfg;
    cfg.type = SmoothingType::Ema;
    cfg.emaTauMs = 10.0f;
    s.apply({0, 0}, 0, cfg);
    Vec2 o;
    // 1 kHz samples for exactly one time constant.
    for (int i = 1; i <= 10; ++i) o = s.apply({1, 0}, i * kMs, cfg);
    EXPECT_NEAR(o.x, 1.0f - std::exp(-1.0f), 1e-3f);  // 63.2%
    EXPECT_TRUE(s.settling());
}

TEST(Smoothing, EmaIsSampleRateIndependent) {
    SmoothingConfig cfg;
    cfg.type = SmoothingType::Ema;
    cfg.emaTauMs = 8.0f;
    Smoother2D slow, fast;
    slow.apply({0, 0}, 0, cfg);
    fast.apply({0, 0}, 0, cfg);
    Vec2 a, b;
    for (int i = 1; i <= 20; ++i) a = slow.apply({1, 0}, i * kMs, cfg);          // 1 kHz
    for (int i = 1; i <= 160; ++i) b = fast.apply({1, 0}, i * kMs / 8, cfg);     // 8 kHz
    EXPECT_NEAR(a.x, b.x, 1e-4f);
}

TEST(Smoothing, OneEuroConvergesAndAdapts) {
    SmoothingConfig cfg;
    cfg.type = SmoothingType::OneEuro;
    cfg.minCutoffHz = 2.0f;
    cfg.beta = 0.5f;
    Smoother2D s;
    s.apply({0, 0}, 0, cfg);
    Vec2 o;
    for (int i = 1; i <= 2000; ++i) o = s.apply({0.5f, -0.5f}, i * kMs, cfg);
    EXPECT_NEAR(o.x, 0.5f, 1e-3f);
    EXPECT_NEAR(o.y, -0.5f, 1e-3f);

    // Faster motion -> less lag: compare tracking error of a fast vs slow ramp.
    auto lagFor = [&](float speedPerSec) {
        Smoother2D f;
        f.apply({0, 0}, 0, cfg);
        Vec2 out;
        for (int i = 1; i <= 100; ++i) out = f.apply({speedPerSec * i / 1000.0f, 0}, i * kMs, cfg);
        const float input = speedPerSec * 0.1f;
        return (input - out.x) / speedPerSec;  // lag in seconds
    };
    EXPECT_LT(lagFor(8.0f), lagFor(0.5f));
}

TEST(Smoothing, FlickBypassPassesFastMotionUnfiltered) {
    SmoothingConfig cfg;
    cfg.type = SmoothingType::Ema;
    cfg.emaTauMs = 30.0f;
    cfg.flickBypassSpeed = 20.0f;  // units per second
    Smoother2D s;
    s.apply({0, 0}, 0, cfg);
    bool bypassed = false;
    const Vec2 o = s.apply({1, 0}, 1 * kMs, cfg, &bypassed);  // 1000 units/s
    EXPECT_TRUE(bypassed);
    EXPECT_FLOAT_EQ(o.x, 1.0f);
    const Vec2 slow = s.apply({1.005f, 0}, 2 * kMs, cfg, &bypassed);  // 5 units/s
    EXPECT_FALSE(bypassed);
    EXPECT_LT(slow.x, 1.005f);
}

TEST(Smoothing, LargeGapReseeds) {
    SmoothingConfig cfg;
    cfg.type = SmoothingType::Ema;
    cfg.emaTauMs = 50.0f;
    Smoother2D s;
    s.apply({0, 0}, 0, cfg);
    const Vec2 o = s.apply({0.7f, 0}, 500 * kMs, cfg);  // 500 ms gap (e.g. reconnect)
    EXPECT_FLOAT_EQ(o.x, 0.7f);
}

TEST(Smoothing, NonMonotonicTimestampDoesNotExplode) {
    SmoothingConfig cfg;
    cfg.type = SmoothingType::OneEuro;
    Smoother2D s;
    s.apply({0.2f, 0}, 10 * kMs, cfg);
    const Vec2 o = s.apply({0.4f, 0}, 5 * kMs, cfg);
    EXPECT_TRUE(std::isfinite(o.x));
    EXPECT_FLOAT_EQ(o.x, 0.4f);  // treated as a re-seed
}
