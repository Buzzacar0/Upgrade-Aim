#pragma once

#include <array>

#include "stickyaim/core/curve.hpp"

namespace sa {

// A magnitude band whose local slope is multiplied by `gain`. Used for the
// "fine adjustment" band near center (gain < 1 = more resolution for
// micro-corrections) and the "fast movement" band near the rim (gain > 1 =
// reach full deflection sooner). The band edges are blended over `blend` so the
// response stays continuous.
struct SlopeZone {
    bool enabled = false;
    float start = 0.0f;
    float end = 0.25f;
    float gain = 1.0f;   // slope multiplier, clamped to [0.05, 20]
    float blend = 0.05f; // transition width on each side
};

// Complete static response for one axis or for the stick magnitude.
struct ResponseShape {
    CurveSpec curve;
    SlopeZone fine{false, 0.0f, 0.25f, 0.6f, 0.08f};
    SlopeZone fast{false, 0.80f, 1.0f, 1.6f, 0.08f};
};

// Compiled lookup table for a ResponseShape. Construction is off the real-time
// path; eval() is branch-light, allocation-free and deterministic.
//
// Guarantees (verified by tests): eval(0) == 0, eval(1) == 1, non-decreasing.
// Slope zones are applied by integrating (curve slope x zone multiplier) and
// renormalizing so full deflection still maps to full output.
class ResponseLut {
public:
    static constexpr int kSize = 4097;

    ResponseLut();  // identity
    static ResponseLut build(const ResponseShape& shape);

    float eval(float t) const noexcept;
    const std::array<float, kSize>& table() const noexcept { return table_; }

private:
    std::array<float, kSize> table_{};
};

// Multiplier contributed by one zone at magnitude t (1 outside the zone).
float zoneMultiplier(const SlopeZone& z, float t) noexcept;

}  // namespace sa
