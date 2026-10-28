#pragma once

#include <memory>
#include <string>
#include <vector>

#include "stickyaim/core/calibration.hpp"
#include "stickyaim/core/profile.hpp"
#include "stickyaim/core/response.hpp"
#include "stickyaim/core/smoothing.hpp"
#include "stickyaim/core/types.hpp"

namespace sa {

struct CompiledContext {
    AimContextConfig cfg;
    ResponseLut radial, x, y;
};

// Immutable, fully precomputed form of a Profile + calibration. Built on a
// non-real-time thread and handed to the engine by pointer swap.
struct CompiledProfile {
    Profile profile;
    DeviceCalibration calibration;
    CompiledContext contexts[kContextCount];

    // Returns nullptr and fills `errors` if the profile does not validate.
    static std::unique_ptr<CompiledProfile> compile(const Profile& p, const DeviceCalibration& cal,
                                                    std::vector<std::string>* errors = nullptr);
    const CompiledContext& context(ContextId id) const noexcept { return contexts[static_cast<int>(id)]; }
};

// Per-stage values for the aim stick, for visualization and tests.
struct ProcessTrace {
    Vec2 raw;          // normalized device value
    Vec2 calibrated;   // after center/range calibration
    Vec2 smoothed;     // after optional smoothing
    Vec2 deadzoned;    // after inner/outer thresholds
    Vec2 curved;       // after response curve
    Vec2 output;       // after sensitivity, saturation and anti-deadzone
    ContextId context = ContextId::Hip;
    float adsBlend = 0.0f;  // 0 = hip response, 1 = ADS response
    bool flickBypass = false;
};

// Static (stateless) shaping of an already-calibrated, already-smoothed aim
// stick vector through one context. Exposed for the curve editor and tests.
Vec2 shapeAimStick(Vec2 calibrated, const CompiledContext& ctx, ProcessTrace* trace = nullptr) noexcept;

// Stateful processor: calibration -> smoothing -> deadzone -> curve ->
// sensitivity -> saturation -> anti-deadzone. process() never allocates.
class Processor {
public:
    // Non-owning. The caller guarantees the profile outlives its use here.
    void setProfile(const CompiledProfile* p) noexcept;
    const CompiledProfile* profile() const noexcept { return profile_; }

    GamepadState process(const RawSample& s, ProcessTrace* trace = nullptr) noexcept;
    void reset() noexcept;

    // True while time-dependent state (smoothing, context crossfade) has not
    // settled; the engine then re-runs the last sample with fresh timestamps.
    bool needsTick() const noexcept;
    ContextId activeContext() const noexcept { return active_; }

private:
    const CompiledProfile* profile_ = nullptr;
    Smoother2D smoother_;
    ContextId active_ = ContextId::Hip;
    float adsBlend_ = 0.0f;
    TimestampNs lastT_ = 0;
    bool primed_ = false;
};

}  // namespace sa
