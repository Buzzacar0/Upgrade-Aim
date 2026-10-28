#include "stickyaim/core/deadzone.hpp"

#include <algorithm>
#include <cmath>

namespace sa {

float rescaleThreshold(float m, float inner, float outer) noexcept {
    if (!(m > inner)) return 0.0f;
    const float span = outer - inner;
    if (!(span > 1e-6f)) return 1.0f;
    return std::min(1.0f, (m - inner) / span);
}

namespace {

float axial(float v, float inner, float outer) noexcept {
    const float m = rescaleThreshold(std::fabs(v), inner, outer);
    return v < 0.0f ? -m : m;
}

}  // namespace

Vec2 applyDeadzone(Vec2 v, const DeadzoneConfig& cfg) noexcept {
    switch (cfg.type) {
        case DeadzoneType::None: {
            const float m = length(v);
            if (m <= 1e-9f) return {};
            const float outer = cfg.outer > 1e-6f ? cfg.outer : 1.0f;
            const float scaled = std::min(1.0f, m / outer);
            return scaledDirection(v, m, scaled);
        }
        case DeadzoneType::Radial: {
            const float m = length(v);
            if (m <= 1e-9f) return {};
            const float scaled = rescaleThreshold(m, cfg.inner, cfg.outer);
            return scaledDirection(v, m, scaled);
        }
        case DeadzoneType::Axial:
            return {axial(v.x, cfg.inner, cfg.outer), axial(v.y, cfg.inner, cfg.outer)};
    }
    return v;
}

std::vector<std::string> validate(const DeadzoneConfig& cfg) {
    std::vector<std::string> errs;
    if (!(cfg.inner >= 0.0f && cfg.inner < 0.9f)) errs.push_back("inner deadzone must be within [0, 0.9)");
    if (!(cfg.outer > 0.1f && cfg.outer <= 1.0f)) errs.push_back("outer threshold must be within (0.1, 1]");
    if (!(cfg.outer > cfg.inner + 0.05f)) errs.push_back("outer threshold must exceed inner deadzone by at least 0.05");
    return errs;
}

const char* toString(DeadzoneType t) noexcept {
    switch (t) {
        case DeadzoneType::None: return "none";
        case DeadzoneType::Radial: return "radial";
        case DeadzoneType::Axial: return "axial";
    }
    return "radial";
}

bool deadzoneTypeFromString(const std::string& s, DeadzoneType& out) noexcept {
    if (s == "none") out = DeadzoneType::None;
    else if (s == "radial") out = DeadzoneType::Radial;
    else if (s == "axial") out = DeadzoneType::Axial;
    else return false;
    return true;
}

}  // namespace sa
