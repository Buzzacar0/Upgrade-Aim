#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "stickyaim/core/types.hpp"

namespace sa {

enum class PollResult {
    NewSample,     // `out` holds a state that differs from the previous one (or the first state)
    NoChange,      // device present, nothing new since the last poll
    Disconnected,  // device absent / unplugged
};

// Acquisition side. Implementations: XInputSource, HidSource (Raw Input),
// ReplaySource (synthetic / recorded data). poll() runs on the real-time thread
// and must not block or allocate.
class IInputSource {
public:
    virtual ~IInputSource() = default;
    virtual std::string name() const = 0;
    virtual bool open() = 0;  // false if the device is not present (the engine will retry)
    virtual void close() = 0;
    virtual PollResult poll(RawSample& out) noexcept = 0;
    // True if RawSample::sequence comes from the device (gaps = missed packets).
    virtual bool hasDeviceSequence() const noexcept { return false; }
};

// Delivery side. Implementations: PreviewSink (no injection), ViGEmX360Sink.
class IOutputSink {
public:
    virtual ~IOutputSink() = default;
    virtual std::string name() const = 0;
    virtual bool open(std::string& error) = 0;
    // Must leave the output in a neutral state (sticks centered, nothing pressed).
    virtual void close() noexcept = 0;
    virtual bool submit(const RawGamepad& pad) noexcept = 0;
    // False for sinks that never reach the OS or a game.
    virtual bool injectsIntoSystem() const noexcept = 0;
    // XInput user index (0-3) occupied by the output device, or -1.
    virtual int occupiedXInputSlot() const noexcept { return -1; }
};

// Safe preview mode: the processed state is computed and visualized but never
// delivered anywhere.
class PreviewSink final : public IOutputSink {
public:
    std::string name() const override { return "Preview (no injection)"; }
    bool open(std::string&) override { return true; }
    void close() noexcept override { last_ = RawGamepad{}; }
    bool submit(const RawGamepad& pad) noexcept override {
        last_ = pad;
        submits_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    bool injectsIntoSystem() const noexcept override { return false; }
    std::uint64_t submits() const noexcept { return submits_.load(std::memory_order_relaxed); }

private:
    RawGamepad last_{};
    std::atomic<std::uint64_t> submits_{0};
};

// Replays samples in real time relative to open(). Loops when `loop` is set.
// Used for the preview mode without hardware, demos and engine tests.
class ReplaySource final : public IInputSource {
public:
    explicit ReplaySource(std::vector<RawSample> samples, bool loop = true, std::string label = "Synthetic replay");
    std::string name() const override { return label_; }
    bool open() override;
    void close() override {}
    PollResult poll(RawSample& out) noexcept override;
    bool hasDeviceSequence() const noexcept override { return true; }

private:
    std::vector<RawSample> samples_;
    bool loop_;
    std::string label_;
    std::size_t index_ = 0;
    TimestampNs start_ = 0;
    TimestampNs span_ = 0;
    std::uint32_t seq_ = 0;
};

}  // namespace sa
