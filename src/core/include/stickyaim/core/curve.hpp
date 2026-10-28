#pragma once

#include <string>
#include <vector>

namespace sa {

enum class CurveType { Linear, Power, Exponential, Custom };

struct CurvePoint {
    float x = 0.0f;
    float y = 0.0f;
};

// Declarative description of a response curve on [0, 1] -> [0, 1].
struct CurveSpec {
    CurveType type = CurveType::Linear;
    float exponent = 1.0f;            // Power: f(t) = t^exponent, exponent in [0.1, 10]
    float expK = 0.0f;                // Exponential: f(t) = (e^(k t) - 1) / (e^k - 1), k in [-12, 12]
    std::vector<CurvePoint> points;   // Custom: interior control points; (0,0) and (1,1) are implicit
};

// An evaluable curve. Every curve produced by fromSpec() satisfies:
//   f(0) == 0, f(1) == 1, f is non-decreasing and continuous on [0, 1].
// Specs that would break those guarantees are sanitized (see sanitize()).
class Curve {
public:
    Curve() = default;
    static Curve fromSpec(const CurveSpec& spec);

    float operator()(float t) const noexcept;
    const CurveSpec& spec() const noexcept { return spec_; }

private:
    CurveSpec spec_;
    // Custom curves: knots (including endpoints) and Fritsch-Carlson tangents.
    std::vector<float> xs_, ys_, ms_;
    float expDenom_ = 1.0f;
};

// Returns a copy of the spec with parameters clamped to their valid ranges and
// custom points sorted, clipped to the unit square and made non-decreasing.
CurveSpec sanitize(const CurveSpec& spec);

// Human-readable problems with a spec (empty if it is already valid as given).
std::vector<std::string> validate(const CurveSpec& spec);

const char* toString(CurveType t) noexcept;
bool curveTypeFromString(const std::string& s, CurveType& out) noexcept;

}  // namespace sa
