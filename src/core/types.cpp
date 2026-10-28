#include "stickyaim/core/types.hpp"

#include <algorithm>

namespace sa {

// Symmetric scaling: +32767 -> +1, -32767 -> -1. -32768 (one count beyond the
// positive range) clamps to -1 so the two half-axes behave identically.
float normalizeAxis(std::int16_t v) noexcept {
    return std::clamp(static_cast<float>(v) / 32767.0f, -1.0f, 1.0f);
}

std::int16_t denormalizeAxis(float v) noexcept {
    if (!std::isfinite(v)) return 0;
    const float c = std::clamp(v, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lround(c * 32767.0f));
}

float normalizeTrigger(std::uint8_t v) noexcept { return static_cast<float>(v) / 255.0f; }

std::uint8_t denormalizeTrigger(float v) noexcept {
    if (!std::isfinite(v)) return 0;
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}

GamepadState normalize(const RawGamepad& raw) noexcept {
    GamepadState s;
    s.buttons = raw.buttons;
    s.lt = normalizeTrigger(raw.leftTrigger);
    s.rt = normalizeTrigger(raw.rightTrigger);
    s.left = {normalizeAxis(raw.lx), normalizeAxis(raw.ly)};
    s.right = {normalizeAxis(raw.rx), normalizeAxis(raw.ry)};
    return s;
}

RawGamepad denormalize(const GamepadState& s) noexcept {
    RawGamepad r;
    r.buttons = s.buttons;
    r.leftTrigger = denormalizeTrigger(s.lt);
    r.rightTrigger = denormalizeTrigger(s.rt);
    r.lx = denormalizeAxis(s.left.x);
    r.ly = denormalizeAxis(s.left.y);
    r.rx = denormalizeAxis(s.right.x);
    r.ry = denormalizeAxis(s.right.y);
    return r;
}

}  // namespace sa
