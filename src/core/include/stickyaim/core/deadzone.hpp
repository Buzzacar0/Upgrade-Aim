#pragma once

#include <string>
#include <vector>

#include "stickyaim/core/types.hpp"

namespace sa {

enum class DeadzoneType {
    None,    // no inner deadzone; only saturation at `outer`
    Radial,  // inner/outer thresholds on the stick vector magnitude (direction preserved)
    Axial,   // inner/outer thresholds on each axis independently
};

struct DeadzoneConfig {
    DeadzoneType type = DeadzoneType::Radial;
    float inner = 0.05f;  // values at or below this read as zero
    float outer = 0.98f;  // values at or above this read as full deflection
};

// Maps a calibrated stick vector to a rescaled vector whose components lie in
// [-1, 1]. Between the thresholds the mapping is linear in magnitude (radial)
// or per component (axial), so no output range is lost to the deadzone.
Vec2 applyDeadzone(Vec2 v, const DeadzoneConfig& cfg) noexcept;

// Scalar version used for triggers and axial mode: maps [inner, outer] -> [0, 1].
float rescaleThreshold(float magnitude, float inner, float outer) noexcept;

std::vector<std::string> validate(const DeadzoneConfig& cfg);

const char* toString(DeadzoneType t) noexcept;
bool deadzoneTypeFromString(const std::string& s, DeadzoneType& out) noexcept;

}  // namespace sa
