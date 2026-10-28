#include "stickyaim/core/pipeline.hpp"

#include <algorithm>
#include <cmath>

#include "stickyaim/core/deadzone.hpp"

namespace sa {

std::unique_ptr<CompiledProfile> CompiledProfile::compile(const Profile& p, const DeviceCalibration& cal,
                                                          std::vector<std::string>* errors) {
    auto errs = validate(p);
    if (!errs.empty()) {
        if (errors) *errors = std::move(errs);
        return nullptr;
    }
    auto out = std::make_unique<CompiledProfile>();
    out->profile = p;
    out->calibration = cal;
    for (int i = 0; i < kContextCount; ++i) {
        const auto& c = p.context(static_cast<ContextId>(i));
        auto& cc = out->contexts[i];
        cc.cfg = c;
        cc.radial = ResponseLut::build(c.radial);
        cc.x = ResponseLut::build(c.x);
        cc.y = ResponseLut::build(c.y);
    }
    return out;
}

Vec2 shapeAimStick(Vec2 v, const CompiledContext& ctx, ProcessTrace* trace) noexcept {
    const auto& c = ctx.cfg;
    const Vec2 dz = applyDeadzone(v, c.deadzone);

    Vec2 curved;
    if (c.curveMode == CurveMode::Radial) {
        const float m = std::min(1.0f, length(dz));
        if (m > 1e-9f) curved = scaledDirection(dz, m, ctx.radial.eval(m));
    } else {
        curved.x = std::copysign(ctx.x.eval(std::fabs(dz.x)), dz.x);
        curved.y = std::copysign(ctx.y.eval(std::fabs(dz.y)), dz.y);
    }

    Vec2 out{curved.x * c.sensX, curved.y * c.sensY};
    if (c.circularSaturation) {
        const float m = length(out);
        if (m > 1.0f) out = scaledDirection(out, m, 1.0f);
    }
    out.x = std::clamp(out.x, -1.0f, 1.0f);
    out.y = std::clamp(out.y, -1.0f, 1.0f);

    if (c.antiDeadzone > 0.0f) {
        const float m = length(out);
        if (m > 1e-6f && m < 1.0f) {
            const float target = c.antiDeadzone + (1.0f - c.antiDeadzone) * m;
            out = scaledDirection(out, m, target);
        }
    }

    if (trace) {
        trace->deadzoned = dz;
        trace->curved = curved;
        trace->output = out;
    }
    return out;
}

void Processor::setProfile(const CompiledProfile* p) noexcept { profile_ = p; }

void Processor::reset() noexcept {
    smoother_.reset();
    active_ = ContextId::Hip;
    adsBlend_ = 0.0f;
    primed_ = false;
}

bool Processor::needsTick() const noexcept {
    if (!profile_) return false;
    const float target = active_ == ContextId::Ads ? 1.0f : 0.0f;
    return smoother_.settling() || adsBlend_ != target;
}

GamepadState Processor::process(const RawSample& s, ProcessTrace* trace) noexcept {
    GamepadState in = normalize(s.pad);
    if (!profile_) return in;
    const CompiledProfile& cp = *profile_;
    const Profile& p = cp.profile;

    // Context selection with hysteresis.
    if (p.adsFromLeftTrigger) {
        if (active_ == ContextId::Hip && in.lt >= p.adsEnter) active_ = ContextId::Ads;
        else if (active_ == ContextId::Ads && in.lt <= p.adsExit) active_ = ContextId::Hip;
    } else {
        active_ = ContextId::Hip;
    }
    const float target = active_ == ContextId::Ads ? 1.0f : 0.0f;
    const double dtMs = primed_ ? static_cast<double>(s.t - lastT_) * 1e-6 : 0.0;
    if (p.contextTransitionMs <= 0.0f || !primed_) {
        adsBlend_ = target;
    } else if (dtMs > 0.0) {
        const float step = static_cast<float>(dtMs / p.contextTransitionMs);
        adsBlend_ = adsBlend_ < target ? std::min(target, adsBlend_ + step) : std::max(target, adsBlend_ - step);
    }
    lastT_ = s.t;
    primed_ = true;

    GamepadState out = in;

    // Movement stick and triggers: calibration + thresholds only.
    out.left = applyDeadzone(applyCalibration(in.left, cp.calibration.left), p.moveStick);
    out.lt = rescaleThreshold(in.lt, p.leftTrigger.inner, p.leftTrigger.outer);
    out.rt = rescaleThreshold(in.rt, p.rightTrigger.inner, p.rightTrigger.outer);

    // Aim stick.
    const Vec2 cal = applyCalibration(in.right, cp.calibration.right);
    const CompiledContext& smoothCtx = cp.context(active_);
    bool flick = false;
    const Vec2 sm = smoother_.apply(cal, s.t, smoothCtx.cfg.smoothing, &flick);

    Vec2 aim;
    if (adsBlend_ <= 0.0f) {
        aim = shapeAimStick(sm, cp.context(ContextId::Hip), trace);
    } else if (adsBlend_ >= 1.0f) {
        aim = shapeAimStick(sm, cp.context(ContextId::Ads), trace);
    } else {
        ProcessTrace hipT, adsT;
        const Vec2 h = shapeAimStick(sm, cp.context(ContextId::Hip), &hipT);
        const Vec2 a = shapeAimStick(sm, cp.context(ContextId::Ads), &adsT);
        aim = lerp(h, a, adsBlend_);
        if (trace) {
            trace->deadzoned = lerp(hipT.deadzoned, adsT.deadzoned, adsBlend_);
            trace->curved = lerp(hipT.curved, adsT.curved, adsBlend_);
            trace->output = aim;
        }
    }
    out.right = aim;

    if (trace) {
        trace->raw = in.right;
        trace->calibrated = cal;
        trace->smoothed = sm;
        trace->context = active_;
        trace->adsBlend = adsBlend_;
        trace->flickBypass = flick;
    }
    return out;
}

}  // namespace sa
