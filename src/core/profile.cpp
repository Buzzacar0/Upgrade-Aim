#include "stickyaim/core/profile.hpp"

#include <cmath>

namespace sa {
namespace {

void prefixed(std::vector<std::string>& out, const std::string& prefix, const std::vector<std::string>& in) {
    for (const auto& e : in) out.push_back(prefix + e);
}

void validateZone(std::vector<std::string>& out, const std::string& prefix, const SlopeZone& z) {
    if (!z.enabled) return;
    if (!(z.start >= 0.0f && z.end <= 1.0f && z.end > z.start)) out.push_back(prefix + "zone needs 0 <= start < end <= 1");
    if (!(z.gain >= 0.05f && z.gain <= 20.0f)) out.push_back(prefix + "zone gain must be within [0.05, 20]");
    if (!(z.blend >= 0.0f && z.blend <= 0.5f)) out.push_back(prefix + "zone blend must be within [0, 0.5]");
}

void validateShape(std::vector<std::string>& out, const std::string& prefix, const ResponseShape& s) {
    prefixed(out, prefix + "curve: ", validate(s.curve));
    validateZone(out, prefix + "fine ", s.fine);
    validateZone(out, prefix + "fast ", s.fast);
}

void validateContext(std::vector<std::string>& out, const std::string& prefix, const AimContextConfig& c) {
    prefixed(out, prefix + "deadzone: ", validate(c.deadzone));
    if (c.curveMode == CurveMode::Radial) {
        validateShape(out, prefix + "radial ", c.radial);
    } else {
        validateShape(out, prefix + "x ", c.x);
        validateShape(out, prefix + "y ", c.y);
    }
    if (!(c.sensX > 0.05f && c.sensX <= 4.0f)) out.push_back(prefix + "horizontal sensitivity must be within (0.05, 4]");
    if (!(c.sensY > 0.05f && c.sensY <= 4.0f)) out.push_back(prefix + "vertical sensitivity must be within (0.05, 4]");
    if (!(c.antiDeadzone >= 0.0f && c.antiDeadzone <= 0.4f)) out.push_back(prefix + "anti-deadzone must be within [0, 0.4]");
    const auto& s = c.smoothing;
    if (s.type == SmoothingType::Ema && !(s.emaTauMs > 0.0f && s.emaTauMs <= 100.0f))
        out.push_back(prefix + "EMA time constant must be within (0, 100] ms");
    if (s.type == SmoothingType::OneEuro && !(s.minCutoffHz > 0.0f && s.dCutoffHz > 0.0f && s.beta >= 0.0f))
        out.push_back(prefix + "One Euro cutoffs must be positive and beta non-negative");
    if (!(s.flickBypassSpeed >= 0.0f)) out.push_back(prefix + "flick bypass speed must be >= 0");
}

}  // namespace

std::vector<std::string> validate(const Profile& p) {
    std::vector<std::string> errs;
    if (p.name.empty()) errs.push_back("profile name must not be empty");
    validateContext(errs, "hip: ", p.hip);
    validateContext(errs, "ads: ", p.ads);
    if (!(p.adsEnter > 0.0f && p.adsEnter <= 1.0f)) errs.push_back("ADS enter threshold must be within (0, 1]");
    if (!(p.adsExit >= 0.0f && p.adsExit < p.adsEnter)) errs.push_back("ADS exit threshold must be below the enter threshold");
    if (!(p.contextTransitionMs >= 0.0f && p.contextTransitionMs <= 500.0f))
        errs.push_back("context transition must be within [0, 500] ms");
    prefixed(errs, "move stick: ", validate(p.moveStick));
    for (const auto* t : {&p.leftTrigger, &p.rightTrigger})
        if (!(t->inner >= 0.0f && t->outer <= 1.0f && t->outer > t->inner + 0.05f))
            errs.push_back("trigger thresholds need 0 <= inner < outer <= 1 with a gap of at least 0.05");
    return errs;
}

const char* toString(CurveMode m) noexcept { return m == CurveMode::PerAxis ? "per_axis" : "radial"; }

bool curveModeFromString(const std::string& s, CurveMode& out) noexcept {
    if (s == "radial") out = CurveMode::Radial;
    else if (s == "per_axis") out = CurveMode::PerAxis;
    else return false;
    return true;
}

const char* toString(ContextId c) noexcept { return c == ContextId::Ads ? "ads" : "hip"; }

}  // namespace sa
