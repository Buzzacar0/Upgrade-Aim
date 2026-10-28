#pragma once

#include <cmath>
#include <cstdint>

namespace sa {

// Monotonic timestamp in nanoseconds. Produced by sa::nowNs() (steady_clock,
// which MSVC implements with QueryPerformanceCounter).
using TimestampNs = std::int64_t;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// hypot is exact for axis-aligned vectors (|(x, 0)| == |x|), which keeps
// radial operations bit-symmetric with axial ones.
inline float length(Vec2 v) noexcept { return std::hypot(v.x, v.y); }
// v / m component-wise: (x / |x|) is exactly +/-1, unlike x * (1 / |x|).
inline Vec2 scaledDirection(Vec2 v, float m, float newLength) noexcept { return {v.x / m * newLength, v.y / m * newLength}; }
inline Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) noexcept { return {a.x * s, a.y * s}; }
inline Vec2 lerp(Vec2 a, Vec2 b, float t) noexcept { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

// Button bitmask. Bit layout is identical to XINPUT_GAMEPAD::wButtons so the
// Windows layer can copy it through unchanged.
enum Button : std::uint16_t {
    DpadUp = 0x0001,
    DpadDown = 0x0002,
    DpadLeft = 0x0004,
    DpadRight = 0x0008,
    Start = 0x0010,
    Back = 0x0020,
    LeftThumb = 0x0040,
    RightThumb = 0x0080,
    LeftShoulder = 0x0100,
    RightShoulder = 0x0200,
    Guide = 0x0400,
    A = 0x1000,
    B = 0x2000,
    X = 0x4000,
    Y = 0x8000,
};

// Device state in XInput units: sticks int16 (+Y is up), triggers 0..255.
struct RawGamepad {
    std::uint16_t buttons = 0;
    std::uint8_t leftTrigger = 0;
    std::uint8_t rightTrigger = 0;
    std::int16_t lx = 0;
    std::int16_t ly = 0;
    std::int16_t rx = 0;
    std::int16_t ry = 0;

    friend bool operator==(const RawGamepad&, const RawGamepad&) = default;
};

struct RawSample {
    TimestampNs t = 0;            // acquisition timestamp (host side, monotonic)
    std::uint32_t sequence = 0;   // device packet/sequence number when the source has one
    RawGamepad pad;
};

// Normalized state: sticks in [-1, 1], triggers in [0, 1].
struct GamepadState {
    std::uint16_t buttons = 0;
    float lt = 0.0f;
    float rt = 0.0f;
    Vec2 left;
    Vec2 right;
};

float normalizeAxis(std::int16_t v) noexcept;
std::int16_t denormalizeAxis(float v) noexcept;
float normalizeTrigger(std::uint8_t v) noexcept;
std::uint8_t denormalizeTrigger(float v) noexcept;

GamepadState normalize(const RawGamepad& raw) noexcept;
RawGamepad denormalize(const GamepadState& s) noexcept;

}  // namespace sa
