#include "stickyaim/core/stats.hpp"

#include <algorithm>
#include <cmath>

namespace sa {

Histogram::Histogram(double binWidth, int bins) : width_(binWidth > 0 ? binWidth : 1.0), bins_(std::max(1, bins), 0) {}

void Histogram::add(double v) noexcept {
    if (!std::isfinite(v)) return;
    ++n_;
    if (n_ == 1) {
        min_ = max_ = v;
    } else {
        min_ = std::min(min_, v);
        max_ = std::max(max_, v);
    }
    const double d = v - mean_;
    mean_ += d / static_cast<double>(n_);
    m2_ += d * (v - mean_);
    const double idx = std::floor(std::max(0.0, v) / width_);
    if (idx >= static_cast<double>(bins_.size())) {
        ++overflow_;
    } else {
        ++bins_[static_cast<std::size_t>(idx)];
    }
}

void Histogram::reset() noexcept {
    std::fill(bins_.begin(), bins_.end(), 0u);
    n_ = overflow_ = 0;
    mean_ = m2_ = min_ = max_ = 0;
}

double Histogram::stddev() const noexcept { return n_ > 1 ? std::sqrt(m2_ / static_cast<double>(n_ - 1)) : 0.0; }

double Histogram::percentile(double p) const noexcept {
    if (n_ == 0) return 0.0;
    const double target = std::clamp(p, 0.0, 100.0) / 100.0 * static_cast<double>(n_);
    std::uint64_t cum = 0;
    for (std::size_t i = 0; i < bins_.size(); ++i) {
        cum += bins_[i];
        if (static_cast<double>(cum) >= target && cum > 0) {
            // Report the bin centre, bounded by the observed range.
            return std::clamp((static_cast<double>(i) + 0.5) * width_, min_, max_);
        }
    }
    return max_;
}

TimingAnalyzer::TimingAnalyzer(double gapFactor, double binWidthUs, int bins) : gapFactor_(gapFactor), hist_(binWidthUs, bins) {}

void TimingAnalyzer::reset() noexcept {
    hist_.reset();
    havePrev_ = false;
    gaps_ = missed_ = 0;
    medianUs_ = 0;
}

void TimingAnalyzer::add(TimestampNs t, std::uint32_t seq, bool hasSeq) noexcept {
    if (havePrev_) {
        const double us = static_cast<double>(t - prevT_) / 1000.0;
        hist_.add(us);
        // Re-estimate the median periodically (cheap: it is a bin scan, and
        // only every 1024 samples).
        if (hist_.count() == 256 || (hist_.count() % 1024) == 0) medianUs_ = hist_.percentile(50.0);
        if (medianUs_ > 0 && us > gapFactor_ * medianUs_) ++gaps_;
        if (hasSeq) {
            const std::uint32_t delta = seq - prevSeq_;
            if (delta > 1 && delta < 0x10000u) missed_ += delta - 1;
        }
    }
    havePrev_ = true;
    prevT_ = t;
    prevSeq_ = seq;
}

TimingSummary TimingAnalyzer::summary() const noexcept {
    TimingSummary s;
    s.samples = hist_.count();
    s.meanUs = hist_.mean();
    s.stddevUs = hist_.stddev();
    s.minUs = hist_.min();
    s.maxUs = hist_.max();
    s.p50Us = hist_.percentile(50);
    s.p95Us = hist_.percentile(95);
    s.p99Us = hist_.percentile(99);
    s.p999Us = hist_.percentile(99.9);
    s.effectiveRateHz = s.meanUs > 0 ? 1e6 / s.meanUs : 0;
    s.gaps = gaps_;
    s.missedSequence = missed_;
    s.gapThresholdUs = gapFactor_ * medianUs_;
    return s;
}

}  // namespace sa
