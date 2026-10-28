#include "stickyaim/core/curve.hpp"

#include <algorithm>
#include <cmath>

namespace sa {
namespace {

constexpr float kMinExponent = 0.1f;
constexpr float kMaxExponent = 10.0f;
constexpr float kMaxAbsK = 12.0f;
constexpr float kLinearK = 1e-4f;

float clamp01(float v) noexcept {
    if (!(v >= 0.0f)) return 0.0f;  // also maps NaN to 0
    return v > 1.0f ? 1.0f : v;
}

}  // namespace

CurveSpec sanitize(const CurveSpec& in) {
    CurveSpec s = in;
    s.exponent = std::isfinite(s.exponent) ? std::clamp(s.exponent, kMinExponent, kMaxExponent) : 1.0f;
    s.expK = std::isfinite(s.expK) ? std::clamp(s.expK, -kMaxAbsK, kMaxAbsK) : 0.0f;

    std::vector<CurvePoint> pts;
    pts.reserve(s.points.size());
    for (const auto& p : s.points) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
        const CurvePoint c{clamp01(p.x), clamp01(p.y)};
        if (c.x <= 0.0f || c.x >= 1.0f) continue;  // endpoints are implicit
        pts.push_back(c);
    }
    std::stable_sort(pts.begin(), pts.end(), [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
    // Collapse points that share an x (keep the last one given).
    std::vector<CurvePoint> unique;
    for (const auto& p : pts) {
        if (!unique.empty() && std::fabs(unique.back().x - p.x) < 1e-6f)
            unique.back() = p;
        else
            unique.push_back(p);
    }
    // Enforce non-decreasing y so the interpolant is monotone.
    float runningMax = 0.0f;
    for (auto& p : unique) {
        runningMax = std::max(runningMax, p.y);
        p.y = runningMax;
    }
    s.points = std::move(unique);
    return s;
}

std::vector<std::string> validate(const CurveSpec& spec) {
    std::vector<std::string> errs;
    if (spec.type == CurveType::Power && !(spec.exponent >= kMinExponent && spec.exponent <= kMaxExponent))
        errs.push_back("power exponent must be within [0.1, 10]");
    if (spec.type == CurveType::Exponential && !(std::fabs(spec.expK) <= kMaxAbsK))
        errs.push_back("exponential k must be within [-12, 12]");
    if (spec.type == CurveType::Custom) {
        float prevX = 0.0f, prevY = 0.0f;
        for (const auto& p : spec.points) {
            if (!(p.x > 0.0f && p.x < 1.0f) || !(p.y >= 0.0f && p.y <= 1.0f)) {
                errs.push_back("custom points must lie inside the unit square, excluding the endpoints");
                break;
            }
            if (p.x <= prevX) {
                errs.push_back("custom points must have strictly increasing x");
                break;
            }
            if (p.y < prevY) {
                errs.push_back("custom points must have non-decreasing y (curve must be monotone)");
                break;
            }
            prevX = p.x;
            prevY = p.y;
        }
    }
    return errs;
}

Curve Curve::fromSpec(const CurveSpec& raw) {
    Curve c;
    c.spec_ = sanitize(raw);
    if (c.spec_.type == CurveType::Exponential && std::fabs(c.spec_.expK) >= kLinearK)
        c.expDenom_ = std::expm1(c.spec_.expK);

    if (c.spec_.type == CurveType::Custom) {
        c.xs_.push_back(0.0f);
        c.ys_.push_back(0.0f);
        for (const auto& p : c.spec_.points) {
            c.xs_.push_back(p.x);
            c.ys_.push_back(p.y);
        }
        c.xs_.push_back(1.0f);
        c.ys_.push_back(std::max(1.0f, c.ys_.back()));

        // Fritsch-Carlson monotone cubic Hermite tangents.
        const size_t n = c.xs_.size();
        std::vector<float> d(n - 1);
        for (size_t i = 0; i + 1 < n; ++i) d[i] = (c.ys_[i + 1] - c.ys_[i]) / (c.xs_[i + 1] - c.xs_[i]);
        c.ms_.assign(n, 0.0f);
        c.ms_[0] = d[0];
        c.ms_[n - 1] = d[n - 2];
        for (size_t i = 1; i + 1 < n; ++i) c.ms_[i] = (d[i - 1] * d[i] <= 0.0f) ? 0.0f : 0.5f * (d[i - 1] + d[i]);
        for (size_t i = 0; i + 1 < n; ++i) {
            if (d[i] == 0.0f) {
                c.ms_[i] = 0.0f;
                c.ms_[i + 1] = 0.0f;
                continue;
            }
            const float a = c.ms_[i] / d[i];
            const float b = c.ms_[i + 1] / d[i];
            const float h = a * a + b * b;
            if (h > 9.0f) {
                const float tau = 3.0f / std::sqrt(h);
                c.ms_[i] = tau * a * d[i];
                c.ms_[i + 1] = tau * b * d[i];
            }
        }
    }
    return c;
}

float Curve::operator()(float tIn) const noexcept {
    const float t = clamp01(tIn);
    switch (spec_.type) {
        case CurveType::Linear:
            return t;
        case CurveType::Power:
            return std::pow(t, spec_.exponent);
        case CurveType::Exponential:
            if (std::fabs(spec_.expK) < kLinearK) return t;
            return clamp01(std::expm1(spec_.expK * t) / expDenom_);
        case CurveType::Custom: {
            if (xs_.size() < 2) return t;
            const auto it = std::upper_bound(xs_.begin(), xs_.end(), t);
            size_t i = static_cast<size_t>(std::distance(xs_.begin(), it));
            i = (i == 0) ? 0 : i - 1;
            if (i >= xs_.size() - 1) i = xs_.size() - 2;
            const float h = xs_[i + 1] - xs_[i];
            const float s = (t - xs_[i]) / h;
            const float s2 = s * s, s3 = s2 * s;
            const float h00 = 2 * s3 - 3 * s2 + 1;
            const float h10 = s3 - 2 * s2 + s;
            const float h01 = -2 * s3 + 3 * s2;
            const float h11 = s3 - s2;
            const float y = h00 * ys_[i] + h10 * h * ms_[i] + h01 * ys_[i + 1] + h11 * h * ms_[i + 1];
            // Clamp to the knot interval: removes float round-off that could
            // otherwise produce tiny non-monotone wiggles.
            return std::clamp(y, ys_[i], ys_[i + 1]);
        }
    }
    return t;
}

const char* toString(CurveType t) noexcept {
    switch (t) {
        case CurveType::Linear: return "linear";
        case CurveType::Power: return "power";
        case CurveType::Exponential: return "exponential";
        case CurveType::Custom: return "custom";
    }
    return "linear";
}

bool curveTypeFromString(const std::string& s, CurveType& out) noexcept {
    if (s == "linear") out = CurveType::Linear;
    else if (s == "power") out = CurveType::Power;
    else if (s == "exponential") out = CurveType::Exponential;
    else if (s == "custom") out = CurveType::Custom;
    else return false;
    return true;
}

}  // namespace sa
