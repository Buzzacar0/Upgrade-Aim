#pragma once

#include <string>
#include <vector>

#include "stickyaim/core/deadzone.hpp"
#include "stickyaim/core/response.hpp"
#include "stickyaim/core/smoothing.hpp"

namespace sa {

enum class CurveMode {
    Radial,   // curve applied to stick magnitude; direction preserved
    PerAxis,  // independent curve per axis (X and Y shapes may differ)
};

enum class ContextId : int { Hip = 0, Ads = 1 };
constexpr int kContextCount = 2;

// Aim-stick processing for one aiming context (hip-fire or ADS).
struct AimContextConfig {
    DeadzoneConfig deadzone{DeadzoneType::Radial, 0.05f, 0.98f};
    CurveMode curveMode = CurveMode::Radial;
    ResponseShape radial;   // used when curveMode == Radial
    ResponseShape x;        // used when curveMode == PerAxis
    ResponseShape y;
    float sensX = 1.0f;     // horizontal output gain (output still saturates at 1)
    float sensY = 1.0f;     // vertical output gain
    SmoothingConfig smoothing;
    bool circularSaturation = true;  // clamp output magnitude to 1 (otherwise per-axis clamp)
    // Minimum output magnitude for any input outside the deadzone. Only useful
    // to cancel a game-side inner deadzone that cannot be set to zero.
    float antiDeadzone = 0.0f;
};

struct TriggerConfig {
    float inner = 0.0f;
    float outer = 1.0f;
};

struct Profile {
    int schemaVersion = 1;
    std::string name = "Default";
    std::string description;
    std::string targetDevice;  // informational, e.g. "GameSir Tarantula 8K"

    AimContextConfig hip;
    AimContextConfig ads;

    // ADS context is selected while the left trigger is held: enter above
    // `adsEnter`, leave below `adsExit` (hysteresis avoids flicker).
    bool adsFromLeftTrigger = true;
    float adsEnter = 0.15f;
    float adsExit = 0.08f;
    // Crossfade time between context responses. 0 = switch on the same sample.
    float contextTransitionMs = 0.0f;

    DeadzoneConfig moveStick{DeadzoneType::Radial, 0.05f, 0.98f};
    TriggerConfig leftTrigger;
    TriggerConfig rightTrigger;

    const AimContextConfig& context(ContextId id) const noexcept { return id == ContextId::Ads ? ads : hip; }
    AimContextConfig& context(ContextId id) noexcept { return id == ContextId::Ads ? ads : hip; }
};

std::vector<std::string> validate(const Profile& p);

const char* toString(CurveMode m) noexcept;
bool curveModeFromString(const std::string& s, CurveMode& out) noexcept;
const char* toString(ContextId c) noexcept;

}  // namespace sa
