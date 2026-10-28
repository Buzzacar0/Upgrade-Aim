#include "stickyaim/core/smoothing.hpp"

#include <algorithm>
#include <cmath>

namespace sa {
namespace {

constexpr double kPi = 3.14159265358979323846;
// Gaps longer than this re-seed the filter instead of being treated as one
// enormous time step (e.g. after a reconnect).
constexpr double kMaxDtSec = 0.1;

float alphaForCutoff(double cutoffHz, double dt) noexcept {
    const double tau = 1.0 / (2.0 * kPi * std::max(cutoffHz, 1e-3));
    return static_cast<float>(1.0 / (1.0 + tau / dt));
}

}  // namespace

Vec2 Smoother2D::apply(Vec2 v, TimestampNs t, const SmoothingConfig& cfg, bool* bypassed) noexcept {
    if (bypassed) *bypassed = false;
    const double dt = primed_ ? static_cast<double>(t - lastT_) * 1e-9 : 0.0;
    if (cfg.type == SmoothingType::None || !primed_ || dt <= 0.0 || dt > kMaxDtSec) {
        primed_ = true;
        lastT_ = t;
        lastIn_ = v;
        out_ = v;
        dOut_ = {};
        return v;
    }

    if (cfg.flickBypassSpeed > 0.0f) {
        const float speed = length(v - lastIn_) / static_cast<float>(dt);
        if (speed >= cfg.flickBypassSpeed) {
            if (bypassed) *bypassed = true;
            lastT_ = t;
            lastIn_ = v;
            out_ = v;
            dOut_ = {};
            return v;
        }
    }

    if (cfg.type == SmoothingType::Ema) {
        const double tau = std::max(1e-6, static_cast<double>(cfg.emaTauMs) * 1e-3);
        const float a = static_cast<float>(1.0 - std::exp(-dt / tau));
        out_ = lerp(out_, v, a);
    } else {  // OneEuro
        const Vec2 rawD = (v - lastIn_) * static_cast<float>(1.0 / dt);
        const float ad = alphaForCutoff(cfg.dCutoffHz, dt);
        dOut_ = lerp(dOut_, rawD, ad);
        const double cutoffX = cfg.minCutoffHz + cfg.beta * std::fabs(dOut_.x);
        const double cutoffY = cfg.minCutoffHz + cfg.beta * std::fabs(dOut_.y);
        const float ax = alphaForCutoff(cutoffX, dt);
        const float ay = alphaForCutoff(cutoffY, dt);
        out_.x += (v.x - out_.x) * ax;
        out_.y += (v.y - out_.y) * ay;
    }
    lastT_ = t;
    lastIn_ = v;
    return out_;
}

bool Smoother2D::settling() const noexcept {
    if (!primed_) return false;
    return std::fabs(out_.x - lastIn_.x) > 1e-5f || std::fabs(out_.y - lastIn_.y) > 1e-5f;
}

const char* toString(SmoothingType t) noexcept {
    switch (t) {
        case SmoothingType::None: return "none";
        case SmoothingType::Ema: return "ema";
        case SmoothingType::OneEuro: return "one_euro";
    }
    return "none";
}

bool smoothingTypeFromString(const std::string& s, SmoothingType& out) noexcept {
    if (s == "none") out = SmoothingType::None;
    else if (s == "ema") out = SmoothingType::Ema;
    else if (s == "one_euro") out = SmoothingType::OneEuro;
    else return false;
    return true;
}

}  // namespace sa
