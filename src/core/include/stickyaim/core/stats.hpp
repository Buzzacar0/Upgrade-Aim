#pragma once

#include <cstdint>
#include <vector>

#include "stickyaim/core/types.hpp"

namespace sa {

// Fixed-bin histogram with running moments. Memory is allocated once in the
// constructor; add() is O(1) and allocation-free.
class Histogram {
public:
    explicit Histogram(double binWidth = 1.0, int bins = 20000);

    void add(double v) noexcept;
    void reset() noexcept;

    std::uint64_t count() const noexcept { return n_; }
    double mean() const noexcept { return n_ ? mean_ : 0.0; }
    double stddev() const noexcept;
    double min() const noexcept { return n_ ? min_ : 0.0; }
    double max() const noexcept { return n_ ? max_ : 0.0; }
    std::uint64_t overflow() const noexcept { return overflow_; }
    // Percentile in [0, 100]. Resolution is one bin width; values beyond the
    // last bin report max().
    double percentile(double p) const noexcept;

    double binWidth() const noexcept { return width_; }
    const std::vector<std::uint32_t>& bins() const noexcept { return bins_; }

private:
    double width_;
    std::vector<std::uint32_t> bins_;
    std::uint64_t n_ = 0, overflow_ = 0;
    double mean_ = 0, m2_ = 0, min_ = 0, max_ = 0;
};

struct TimingSummary {
    std::uint64_t samples = 0;
    double meanUs = 0, stddevUs = 0, minUs = 0, maxUs = 0;
    double p50Us = 0, p95Us = 0, p99Us = 0, p999Us = 0;
    double effectiveRateHz = 0;     // 1e6 / mean interval
    std::uint64_t gaps = 0;         // intervals above the gap threshold
    std::uint64_t missedSequence = 0;  // packets skipped according to the device sequence number
    double gapThresholdUs = 0;
};

// Analyzes the arrival times of samples (or reports). Reports what was
// measured only; never assumes a nominal rate.
class TimingAnalyzer {
public:
    // gapFactor: an interval counts as a gap when it exceeds gapFactor x the
    // median interval (median estimated once enough samples are available).
    explicit TimingAnalyzer(double gapFactor = 3.0, double binWidthUs = 1.0, int bins = 50000);

    void add(TimestampNs t, std::uint32_t sequence, bool hasSequence) noexcept;
    void reset() noexcept;
    TimingSummary summary() const noexcept;
    const Histogram& intervals() const noexcept { return hist_; }

private:
    double gapFactor_;
    Histogram hist_;
    bool havePrev_ = false;
    TimestampNs prevT_ = 0;
    std::uint32_t prevSeq_ = 0;
    std::uint64_t gaps_ = 0, missed_ = 0;
    double medianUs_ = 0;
};

}  // namespace sa
