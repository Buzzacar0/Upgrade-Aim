#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <thread>

#include "stickyaim/core/signals.hpp"
#include "stickyaim/core/spsc_queue.hpp"
#include "stickyaim/core/stats.hpp"

using namespace sa;

TEST(Histogram, MomentsAndPercentiles) {
    Histogram h(1.0, 1000);
    for (int i = 1; i <= 100; ++i) h.add(i);
    EXPECT_EQ(h.count(), 100u);
    EXPECT_DOUBLE_EQ(h.mean(), 50.5);
    EXPECT_DOUBLE_EQ(h.min(), 1.0);
    EXPECT_DOUBLE_EQ(h.max(), 100.0);
    EXPECT_NEAR(h.percentile(50), 50.5, 1.0);
    EXPECT_NEAR(h.percentile(99), 99.5, 1.0);
    EXPECT_NEAR(h.stddev(), 29.01, 0.01);
}

TEST(Histogram, OverflowAndNonFinite) {
    Histogram h(1.0, 10);
    h.add(5);
    h.add(500);
    h.add(std::nan(""));
    EXPECT_EQ(h.count(), 2u);
    EXPECT_EQ(h.overflow(), 1u);
    EXPECT_DOUBLE_EQ(h.percentile(100), 500.0);
}

TEST(Timing, MeasuresIntervalAndJitter) {
    SignalSpec spec;
    spec.rateHz = 1000.0;
    spec.durationSec = 5.0;
    spec.timingJitterUs = 20.0;
    TimingAnalyzer ta;
    for (const auto& s : generate(spec)) ta.add(s.t, s.sequence, true);
    const auto sum = ta.summary();
    EXPECT_NEAR(sum.meanUs, 1000.0, 1.0);
    EXPECT_NEAR(sum.effectiveRateHz, 1000.0, 1.0);
    // Interval jitter of independent timestamp jitter is sqrt(2) * sigma.
    EXPECT_NEAR(sum.stddevUs, 20.0 * std::sqrt(2.0), 3.0);
    EXPECT_EQ(sum.missedSequence, 0u);
}

TEST(Timing, DetectsDroppedSamplesAndGaps) {
    SignalSpec spec;
    spec.rateHz = 1000.0;
    spec.durationSec = 10.0;
    spec.dropProbability = 0.01;
    const auto samples = generate(spec);
    const std::size_t dropped = 10000 - samples.size();
    ASSERT_GT(dropped, 50u);
    TimingAnalyzer ta(1.5);
    for (const auto& s : samples) ta.add(s.t, s.sequence, true);
    const auto sum = ta.summary();
    // First sample may have been dropped; that one cannot be detected.
    EXPECT_NEAR(static_cast<double>(sum.missedSequence), static_cast<double>(dropped), 1.0);
    EXPECT_GT(sum.gaps, dropped / 2);
}

TEST(Timing, NoNominalRateAssumed) {
    // 8 kHz nominal but the source only delivers 1 kHz: report 1 kHz.
    TimingAnalyzer ta;
    for (int i = 0; i < 5000; ++i) ta.add(static_cast<TimestampNs>(i) * 1'000'000, 0, false);
    EXPECT_NEAR(ta.summary().effectiveRateHz, 1000.0, 0.5);
}

TEST(SpscQueue, PreservesOrderUnderConcurrency) {
    SpscQueue<std::uint64_t> q(1024);
    constexpr std::uint64_t kN = 2'000'000;
    std::atomic<bool> ok{true};
    std::thread consumer([&] {
        std::uint64_t expected = 0, v = 0;
        while (expected < kN) {
            if (q.pop(v)) {
                if (v != expected) ok = false;
                ++expected;
            }
        }
    });
    for (std::uint64_t i = 0; i < kN;) {
        if (q.push(i)) ++i;
    }
    consumer.join();
    EXPECT_TRUE(ok);
}

TEST(SpscQueue, ReportsFull) {
    SpscQueue<int> q(4);
    EXPECT_EQ(q.capacity(), 4u);
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(q.push(i));
    EXPECT_FALSE(q.push(99));
    int v = -1;
    EXPECT_TRUE(q.pop(v));
    EXPECT_EQ(v, 0);
    EXPECT_TRUE(q.push(99));
}
