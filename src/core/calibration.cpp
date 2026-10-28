#include "stickyaim/core/calibration.hpp"

#include <algorithm>
#include <cmath>

namespace sa {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kSafety = 1.2f;
}  // namespace

Vec2 applyCalibration(Vec2 v, const StickCalibration& c) noexcept {
    auto axis = [](float value, float center, float maxPos, float maxNeg) noexcept {
        const float d = value - center;
        const float range = d >= 0.0f ? (maxPos - center) : (maxNeg + center);
        if (!(range > 0.05f)) return std::clamp(d, -1.0f, 1.0f);
        return std::clamp(d / range, -1.5f, 1.5f);
    };
    return {axis(v.x, c.centerX, c.maxPosX, c.maxNegX), axis(v.y, c.centerY, c.maxPosY, c.maxNegY)};
}

// ---------------------------------------------------------------- RestAnalyzer

void RestAnalyzer::reset() { *this = RestAnalyzer{}; }

void RestAnalyzer::add(Vec2 v) {
    if (n_ > 0) {
        const Vec2 d = v - prev_;
        diffSq_ += static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y;
    }
    prev_ = v;
    ++n_;
    sumX_ += v.x;
    sumY_ += v.y;
    sumXX_ += static_cast<double>(v.x) * v.x;
    sumYY_ += static_cast<double>(v.y) * v.y;
    samples_.push_back(v);
}

RestReport RestAnalyzer::report() const {
    RestReport r;
    r.samples = n_;
    if (n_ == 0) return r;
    const double n = static_cast<double>(n_);
    const double mx = sumX_ / n, my = sumY_ / n;
    r.mean = {static_cast<float>(mx), static_cast<float>(my)};
    r.stddev = {static_cast<float>(std::sqrt(std::max(0.0, sumXX_ / n - mx * mx))),
                static_cast<float>(std::sqrt(std::max(0.0, sumYY_ / n - my * my)))};
    for (const auto& s : samples_) {
        r.maxRadiusFromZero = std::max(r.maxRadiusFromZero, length(s));
        r.maxRadiusFromMean = std::max(r.maxRadiusFromMean, length(s - r.mean));
    }
    r.diffRms = n_ > 1 ? static_cast<float>(std::sqrt(diffSq_ / (n - 1))) : 0.0f;
    r.recommendedInnerUncalibrated = std::clamp(r.maxRadiusFromZero * kSafety, 0.01f, 0.5f);
    r.recommendedInnerCalibrated = std::clamp(r.maxRadiusFromMean * kSafety, 0.01f, 0.5f);
    r.driftSuspected = length(r.mean) > 0.06f;
    r.noisy = r.diffRms > 0.004f;
    return r;
}

// --------------------------------------------------------------- RangeAnalyzer

void RangeAnalyzer::reset() { *this = RangeAnalyzer{}; }

void RangeAnalyzer::add(Vec2 v) {
    if (!any_) {
        r_.minX = r_.maxX = v.x;
        r_.minY = r_.maxY = v.y;
        any_ = true;
    }
    ++r_.samples;
    r_.minX = std::min(r_.minX, v.x);
    r_.maxX = std::max(r_.maxX, v.x);
    r_.minY = std::min(r_.minY, v.y);
    r_.maxY = std::max(r_.maxY, v.y);
    const float m = length(v);
    if (m < 0.3f) return;  // direction is meaningless near center
    float a = std::atan2(v.y, v.x);
    if (a < 0) a += 2 * kPi;
    const int s = std::min(RangeReport::kSectors - 1, static_cast<int>(a / (2 * kPi) * RangeReport::kSectors));
    r_.sectorMax[s] = std::max(r_.sectorMax[s], m);
}

RangeReport RangeAnalyzer::report() const {
    RangeReport r = r_;
    float mn = 1e9f, mx = 0, sum = 0;
    for (float s : r.sectorMax) {
        if (s <= 0) continue;
        ++r.sectorsCovered;
        mn = std::min(mn, s);
        mx = std::max(mx, s);
        sum += s;
    }
    if (r.sectorsCovered > 0) {
        r.minSectorMax = mn;
        r.meanSectorMax = sum / r.sectorsCovered;
        r.circularityError = (mx - mn) / r.meanSectorMax;
        r.recommendedOuter = std::clamp(mn * 0.98f, 0.5f, 1.0f);
    }
    return r;
}

StickCalibration RangeAnalyzer::toCalibration(Vec2 center) const {
    StickCalibration c;
    c.centerX = center.x;
    c.centerY = center.y;
    if (!any_) return c;
    // Only trust an extreme if the stick clearly travelled there.
    auto pick = [](float measured, float fallback) { return measured > 0.5f ? std::min(measured, 1.0f) : fallback; };
    c.maxPosX = pick(r_.maxX, 1.0f);
    c.maxNegX = pick(-r_.minX, 1.0f);
    c.maxPosY = pick(r_.maxY, 1.0f);
    c.maxNegY = pick(-r_.minY, 1.0f);
    return c;
}

// ---------------------------------------------------------------- DriftMonitor

void DriftMonitor::add(Vec2 v, TimestampNs t) noexcept {
    constexpr float kWindowAlpha = 0.02f;
    constexpr float kStillVar = 2.5e-5f;        // (0.005)^2
    constexpr TimestampNs kStillNs = 1'000'000'000;
    constexpr float kRestAlpha = 0.001f;

    const Vec2 d = v - windowMean_;
    windowMean_ = windowMean_ + d * kWindowAlpha;
    windowVar_ = (1 - kWindowAlpha) * (windowVar_ + kWindowAlpha * (d.x * d.x + d.y * d.y));
    prev_ = v;

    const bool candidate = length(v) < restRadius_ && windowVar_ < kStillVar;
    if (!candidate) {
        still_ = false;
        return;
    }
    if (!still_) {
        still_ = true;
        stillSince_ = t;
        return;
    }
    if (t - stillSince_ < kStillNs) return;
    if (!haveRest_) {
        rest_ = windowMean_;
        haveRest_ = true;
    } else {
        rest_ = lerp(rest_, v, kRestAlpha);
    }
}

float DriftMonitor::deadzoneUsage(float inner) const noexcept {
    if (!haveRest_ || inner <= 0) return 0;
    return length(rest_) / inner;
}

}  // namespace sa
