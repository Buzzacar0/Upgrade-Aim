#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "stickyaim/core/types.hpp"

namespace sa {

// Per-stick calibration. Center is subtracted, then each half-axis is rescaled
// so the measured physical extreme maps to exactly +/-1.
struct StickCalibration {
    float centerX = 0.0f;
    float centerY = 0.0f;
    float maxPosX = 1.0f;  // largest raw normalized value reached on +X
    float maxNegX = 1.0f;  // magnitude of the most negative raw value on -X
    float maxPosY = 1.0f;
    float maxNegY = 1.0f;
};

struct DeviceCalibration {
    std::string deviceKey;  // e.g. "3537:103D" or "xinput:0"
    StickCalibration left;
    StickCalibration right;
};

Vec2 applyCalibration(Vec2 v, const StickCalibration& c) noexcept;

// --- Rest analysis: user leaves the stick untouched for a few seconds. -------

struct RestReport {
    std::size_t samples = 0;
    Vec2 mean;                  // resting offset (= center to calibrate out)
    Vec2 stddev;                // per-axis noise
    float maxRadiusFromZero = 0;   // largest |v| seen (what an uncalibrated deadzone must cover)
    float maxRadiusFromMean = 0;   // largest |v - mean| (what a calibrated deadzone must cover)
    float diffRms = 0;          // RMS of successive differences: high-frequency jitter
    // Recommendations (radial, normalized units). Include a 20% safety margin.
    float recommendedInnerUncalibrated = 0;
    float recommendedInnerCalibrated = 0;
    bool driftSuspected = false;   // resting offset larger than typical (> 0.06)
    bool noisy = false;            // diffRms > 0.004 (about 130 raw counts)
};

class RestAnalyzer {
public:
    void reset();
    void add(Vec2 v);
    RestReport report() const;
    std::size_t count() const noexcept { return n_; }

private:
    std::size_t n_ = 0;
    double sumX_ = 0, sumY_ = 0, sumXX_ = 0, sumYY_ = 0;
    double diffSq_ = 0;
    Vec2 prev_{};
    std::vector<Vec2> samples_;  // kept to compute radius from the final mean
};

// --- Range analysis: user rotates the stick around the gate several times. ---

struct RangeReport {
    std::size_t samples = 0;
    float minX = 0, maxX = 0, minY = 0, maxY = 0;
    static constexpr int kSectors = 36;
    std::array<float, kSectors> sectorMax{};  // max magnitude per 10-degree sector
    int sectorsCovered = 0;
    float minSectorMax = 0;    // weakest direction
    float meanSectorMax = 0;
    float circularityError = 0;  // (max - min) / mean over covered sectors
    float recommendedOuter = 0;  // so every covered direction can reach full output
};

class RangeAnalyzer {
public:
    void reset();
    void add(Vec2 v);
    RangeReport report() const;
    StickCalibration toCalibration(Vec2 center) const;

private:
    RangeReport r_{};
    bool any_ = false;
};

// --- Continuous drift monitor (runs on every sample, allocation-free). -------

class DriftMonitor {
public:
    // restRadius: samples with |v| below this are candidates for "at rest".
    explicit DriftMonitor(float restRadius = 0.2f) : restRadius_(restRadius) {}
    void add(Vec2 v, TimestampNs t) noexcept;
    Vec2 restEstimate() const noexcept { return rest_; }
    bool hasEstimate() const noexcept { return haveRest_; }
    // Fraction of the given inner deadzone consumed by the current rest offset.
    float deadzoneUsage(float innerDeadzone) const noexcept;

private:
    float restRadius_;
    Vec2 rest_{};
    bool haveRest_ = false;
    Vec2 windowMean_{};
    float windowVar_ = 0;
    TimestampNs stillSince_ = 0;
    bool still_ = false;
    Vec2 prev_{};
};

}  // namespace sa
