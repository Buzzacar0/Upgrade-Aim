#include "stickyaim/core/io.hpp"

#include "stickyaim/core/clock.hpp"

namespace sa {

ReplaySource::ReplaySource(std::vector<RawSample> samples, bool loop, std::string label)
    : samples_(std::move(samples)), loop_(loop), label_(std::move(label)) {
    if (!samples_.empty()) {
        const TimestampNs t0 = samples_.front().t;
        for (auto& s : samples_) s.t -= t0;
        // One nominal interval past the last sample so loops are evenly spaced.
        const TimestampNs step = samples_.size() > 1 ? samples_[1].t - samples_[0].t : 1'000'000;
        span_ = samples_.back().t + step;
    }
}

bool ReplaySource::open() {
    if (samples_.empty()) return false;
    index_ = 0;
    seq_ = 0;
    start_ = nowNs();
    return true;
}

PollResult ReplaySource::poll(RawSample& out) noexcept {
    if (samples_.empty()) return PollResult::Disconnected;
    if (index_ >= samples_.size()) {
        if (!loop_) return PollResult::NoChange;
        index_ = 0;
        start_ += span_;
    }
    const TimestampNs now = nowNs();
    const TimestampNs due = start_ + samples_[index_].t;
    if (now < due) return PollResult::NoChange;
    // If we fell behind, skip to the newest due sample (as a real device would).
    while (index_ + 1 < samples_.size() && start_ + samples_[index_ + 1].t <= now) {
        ++index_;
        ++seq_;
    }
    out = samples_[index_];
    out.t = now;
    out.sequence = ++seq_;
    ++index_;
    return PollResult::NewSample;
}

}  // namespace sa
