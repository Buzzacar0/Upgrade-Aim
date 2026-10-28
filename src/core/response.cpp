#include "stickyaim/core/response.hpp"

#include <algorithm>
#include <cmath>

namespace sa {
namespace {

float smoothstep(float e0, float e1, float x) noexcept {
    if (e1 <= e0) return x >= e1 ? 1.0f : 0.0f;
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float zoneMultiplier(const SlopeZone& z, float t) noexcept {
    if (!z.enabled) return 1.0f;
    const float gain = std::clamp(z.gain, 0.05f, 20.0f);
    const float blend = std::max(0.0f, z.blend);
    // Weight is 1 inside [start, end] and fades to 0 over `blend` outside it.
    // Zones touching the domain edges do not fade at that edge.
    const float rise = z.start <= 0.0f ? 1.0f : smoothstep(z.start - blend, z.start, t);
    const float fall = z.end >= 1.0f ? 1.0f : 1.0f - smoothstep(z.end, z.end + blend, t);
    const float w = std::min(rise, fall);
    return 1.0f + (gain - 1.0f) * w;
}

ResponseLut::ResponseLut() {
    for (int i = 0; i < kSize; ++i) table_[i] = static_cast<float>(i) / (kSize - 1);
}

ResponseLut ResponseLut::build(const ResponseShape& shape) {
    ResponseLut lut;
    const Curve curve = Curve::fromSpec(shape.curve);
    const bool zones = shape.fine.enabled || shape.fast.enabled;

    if (!zones) {
        for (int i = 0; i < kSize; ++i) lut.table_[i] = curve(static_cast<float>(i) / (kSize - 1));
        lut.table_[0] = 0.0f;
        lut.table_[kSize - 1] = 1.0f;
        return lut;
    }

    // Integrate the zone-weighted slope in double precision.
    std::array<double, kSize> acc{};
    double prev = curve(0.0f);
    for (int i = 1; i < kSize; ++i) {
        const float t = static_cast<float>(i) / (kSize - 1);
        const float mid = (static_cast<float>(i) - 0.5f) / (kSize - 1);
        const double f = curve(t);
        const double delta = std::max(0.0, f - prev);
        prev = f;
        const double mult = static_cast<double>(zoneMultiplier(shape.fine, mid)) * zoneMultiplier(shape.fast, mid);
        acc[i] = acc[i - 1] + delta * mult;
    }
    const double total = acc[kSize - 1];
    for (int i = 0; i < kSize; ++i) {
        const double v = total > 0.0 ? acc[i] / total : static_cast<double>(i) / (kSize - 1);
        lut.table_[i] = static_cast<float>(std::clamp(v, 0.0, 1.0));
    }
    lut.table_[0] = 0.0f;
    lut.table_[kSize - 1] = 1.0f;
    return lut;
}

float ResponseLut::eval(float t) const noexcept {
    if (!(t > 0.0f)) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float pos = t * (kSize - 1);
    const int i = static_cast<int>(pos);
    const float frac = pos - static_cast<float>(i);
    return table_[i] + (table_[i + 1] - table_[i]) * frac;
}

}  // namespace sa
