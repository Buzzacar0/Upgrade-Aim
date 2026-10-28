#pragma once

#include <string>

#include "stickyaim/core/types.hpp"

namespace sa {

enum class SmoothingType {
    None,     // pass-through, zero added latency
    Ema,      // first-order low-pass with time constant tau (time-step aware)
    OneEuro,  // adaptive low-pass (Casiez et al. 2012): heavy at rest, light when moving fast
};

struct SmoothingConfig {
    SmoothingType type = SmoothingType::None;
    float emaTauMs = 4.0f;
    float minCutoffHz = 3.0f;  // One Euro: cutoff at zero speed
    float beta = 0.05f;        // One Euro: cutoff increase per unit speed (units/s)
    float dCutoffHz = 10.0f;   // One Euro: derivative filter cutoff
    // When the stick moves faster than this (normalized units per second) the
    // filter is bypassed and re-seeded so flicks are never delayed. 0 = off.
    float flickBypassSpeed = 0.0f;
};

// Two-axis filter. All behavior depends only on sample values and timestamps,
// so identical input sequences always give identical output.
class Smoother2D {
public:
    void reset() noexcept { primed_ = false; }

    // Returns the filtered value. `bypassed` (optional) reports whether the
    // flick bypass fired for this sample.
    Vec2 apply(Vec2 v, TimestampNs t, const SmoothingConfig& cfg, bool* bypassed = nullptr) noexcept;

    // True while the output has not yet converged on the last input. The
    // engine uses this to keep ticking the filter when the device is idle.
    bool settling() const noexcept;

private:
    bool primed_ = false;
    TimestampNs lastT_ = 0;
    Vec2 lastIn_{};
    Vec2 out_{};
    Vec2 dOut_{};  // One Euro: filtered derivative
};

const char* toString(SmoothingType t) noexcept;
bool smoothingTypeFromString(const std::string& s, SmoothingType& out) noexcept;

}  // namespace sa
