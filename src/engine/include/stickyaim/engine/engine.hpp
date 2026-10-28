#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "stickyaim/core/io.hpp"
#include "stickyaim/core/pipeline.hpp"
#include "stickyaim/core/spsc_queue.hpp"

namespace sa {

enum class PollMode {
    Spin,    // poll continuously; lowest latency, keeps one core busy
    Hybrid,  // sleep on a high-resolution timer, spin the last ~150 us
    Timer,   // sleep only; lowest CPU, more wake-up jitter
};

struct PanicCombo {
    bool enabled = true;
    std::uint16_t buttons = Button::Back | Button::Start;
    int holdMs = 1500;
};

struct EngineConfig {
    PollMode mode = PollMode::Hybrid;
    int intervalUs = 125;          // poll period for Hybrid/Timer (125 us = 8 kHz; matches 8 kHz-class controllers)
    bool useMmcss = true;
    bool tickWhileIdle = true;     // keep smoothing/crossfade converging when the device sends nothing new
    int reconnectIntervalMs = 250;
    PanicCombo panic;              // hold to toggle pass-through
};

enum class EngineState : int { Stopped, WaitingForDevice, Running, Faulted };

// One processed sample, pushed to the telemetry queue for the UI / logger.
struct TelemetrySample {
    TimestampNs t;           // acquisition timestamp
    std::uint32_t sequence;
    RawGamepad raw;
    RawGamepad out;
    std::uint32_t processNs;  // Processor::process() duration
    std::uint32_t submitNs;   // sink submit duration
    std::uint32_t totalNs;    // acquisition -> submit complete (host software path only)
    std::uint32_t pollGapNs;  // time since the previous poll: the new state appeared somewhere in this window (0 = unknown)
    std::uint8_t context;     // ContextId
    std::uint8_t flags;       // TelemetryFlag bits
};

enum TelemetryFlag : std::uint8_t {
    kFlagBypass = 1,
    kFlagFlick = 2,
    kFlagIdleTick = 4,
    kFlagSinkError = 8,
    kFlagDisconnect = 16,
    kFlagReconnect = 32,
};

struct EngineCounters {
    EngineState state = EngineState::Stopped;
    std::uint64_t loops = 0;
    std::uint64_t newSamples = 0;
    std::uint64_t idleTicks = 0;
    std::uint64_t missedSequence = 0;   // device packets the loop never saw
    std::uint64_t telemetryDrops = 0;
    std::uint64_t sinkErrors = 0;
    std::uint64_t disconnects = 0;
    std::uint64_t lateWakeups = 0;      // loop woke > 2x interval late
    std::uint64_t maxLateUs = 0;
    std::uint64_t profileSwaps = 0;
    bool bypass = false;
    bool injecting = false;
    std::string priority;
};

// Real-time input loop: source -> Processor -> sink, on a dedicated thread.
//
// Threading contract:
//  * The input thread never allocates, locks, or frees after start().
//  * publishProfile() may be called from any one control thread at any time.
//    Profiles are swapped by pointer; retired profiles are freed on the
//    control thread, never on the input thread.
//  * stop() always leaves the sink neutral and closed.
class Engine {
public:
    Engine(std::unique_ptr<IInputSource> source, std::unique_ptr<IOutputSink> sink, EngineConfig cfg = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool start(std::string& error);
    void stop();
    bool running() const noexcept { return running_.load(std::memory_order_acquire); }

    void publishProfile(std::unique_ptr<CompiledProfile> profile);
    void setBypass(bool on) noexcept { bypass_.store(on, std::memory_order_relaxed); }
    bool bypass() const noexcept { return bypass_.load(std::memory_order_relaxed); }

    SpscQueue<TelemetrySample>& telemetry() noexcept { return telemetry_; }
    EngineCounters counters() const;
    const IInputSource& source() const noexcept { return *source_; }
    const IOutputSink& sink() const noexcept { return *sink_; }

private:
    void run();
    void collectRetired();
    void emit(const TelemetrySample& s) noexcept;

    std::unique_ptr<IInputSource> source_;
    std::unique_ptr<IOutputSink> sink_;
    EngineConfig cfg_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> bypass_{false};

    std::atomic<CompiledProfile*> pending_{nullptr};
    CompiledProfile* active_ = nullptr;  // owned; touched by the input thread only while running
    SpscQueue<CompiledProfile*> retired_{64};

    SpscQueue<TelemetrySample> telemetry_{1 << 16};

    std::atomic<int> state_{static_cast<int>(EngineState::Stopped)};
    std::atomic<std::uint64_t> loops_{0}, newSamples_{0}, idleTicks_{0}, missed_{0}, telemetryDrops_{0}, sinkErrors_{0},
        disconnects_{0}, lateWakeups_{0}, maxLateUs_{0}, swaps_{0};
    mutable std::mutex priorityMu_;  // guards priority_ (written once by the input thread before its loop)
    std::string priority_;
};

const char* toString(EngineState s) noexcept;
const char* toString(PollMode m) noexcept;

}  // namespace sa
