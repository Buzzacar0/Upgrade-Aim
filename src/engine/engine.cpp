#include "stickyaim/engine/engine.hpp"

#include <algorithm>

#include "stickyaim/core/clock.hpp"
#include "stickyaim/core/logger.hpp"
#include "stickyaim/platform/win_util.hpp"

namespace sa {
namespace {

std::uint32_t clampU32(TimestampNs v) noexcept {
    return static_cast<std::uint32_t>(std::clamp<TimestampNs>(v, 0, 0xFFFFFFFFll));
}

}  // namespace

Engine::Engine(std::unique_ptr<IInputSource> source, std::unique_ptr<IOutputSink> sink, EngineConfig cfg)
    : source_(std::move(source)), sink_(std::move(sink)), cfg_(cfg) {
    cfg_.intervalUs = std::clamp(cfg_.intervalUs, 50, 20000);
    cfg_.reconnectIntervalMs = std::clamp(cfg_.reconnectIntervalMs, 10, 5000);
}

Engine::~Engine() {
    stop();
    collectRetired();
    delete pending_.exchange(nullptr);
    delete active_;
    active_ = nullptr;
}

bool Engine::start(std::string& error) {
    if (running()) return true;
    if (!sink_->open(error)) return false;
    stopRequested_.store(false);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
    return true;
}

void Engine::stop() {
    if (!thread_.joinable()) return;
    stopRequested_.store(true);
    thread_.join();
    running_.store(false, std::memory_order_release);
    state_.store(static_cast<int>(EngineState::Stopped));
    collectRetired();
}

void Engine::publishProfile(std::unique_ptr<CompiledProfile> profile) {
    collectRetired();
    // If the previous pending profile was never picked up, it is ours to free.
    delete pending_.exchange(profile.release(), std::memory_order_acq_rel);
}

void Engine::collectRetired() {
    CompiledProfile* p = nullptr;
    while (retired_.pop(p)) delete p;
}

void Engine::emit(const TelemetrySample& s) noexcept {
    if (!telemetry_.push(s)) telemetryDrops_.fetch_add(1, std::memory_order_relaxed);
}

EngineCounters Engine::counters() const {
    EngineCounters c;
    c.state = static_cast<EngineState>(state_.load());
    c.loops = loops_.load();
    c.newSamples = newSamples_.load();
    c.idleTicks = idleTicks_.load();
    c.missedSequence = missed_.load();
    c.telemetryDrops = telemetryDrops_.load();
    c.sinkErrors = sinkErrors_.load();
    c.disconnects = disconnects_.load();
    c.lateWakeups = lateWakeups_.load();
    c.maxLateUs = maxLateUs_.load();
    c.profileSwaps = swaps_.load();
    c.bypass = bypass();
    c.injecting = running() && sink_->injectsIntoSystem();
    {
        std::lock_guard lk(priorityMu_);
        c.priority = priority_;
    }
    return c;
}

void Engine::run() {
    {
        std::string applied = win::elevateCurrentThread(cfg_.useMmcss);
        std::lock_guard lk(priorityMu_);
        priority_ = std::move(applied);
    }
    win::PreciseWaiter waiter;
    Processor proc;
    const RawGamepad neutral{};
    const TimestampNs interval = static_cast<TimestampNs>(cfg_.intervalUs) * 1000;
    const TimestampNs spin = cfg_.mode == PollMode::Hybrid ? std::min<TimestampNs>(150'000, interval) : 0;

    bool connected = source_->open();
    state_.store(static_cast<int>(connected ? EngineState::Running : EngineState::WaitingForDevice));
    if (!connected) logInfo("engine", "input device not present, waiting: " + source_->name());

    RawSample last{};
    bool haveLast = false;
    bool haveSeq = false;
    std::uint32_t lastSeq = 0;
    TimestampNs panicSince = 0;
    bool panicLatched = false;
    TimestampNs nextDeadline = nowNs();
    TimestampNs nextReconnect = 0;
    TimestampNs prevPollT = 0;  // 0 = no previous poll since (re)connect

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        // --- pacing
        if (cfg_.mode != PollMode::Spin || !connected) {
            nextDeadline += connected ? interval : static_cast<TimestampNs>(cfg_.reconnectIntervalMs) * 1'000'000 / 10;
            const TimestampNs now = nowNs();
            if (nextDeadline < now - interval) {
                // Woke very late (scheduler). Record and resynchronize instead of bursting.
                const auto lateUs = static_cast<std::uint64_t>((now - nextDeadline) / 1000);
                lateWakeups_.fetch_add(1, std::memory_order_relaxed);
                if (lateUs > maxLateUs_.load(std::memory_order_relaxed)) maxLateUs_.store(lateUs, std::memory_order_relaxed);
                nextDeadline = now;
            } else {
                waiter.waitUntil(nextDeadline, cfg_.mode == PollMode::Timer ? 0 : spin);
            }
        }
        loops_.fetch_add(1, std::memory_order_relaxed);

        // --- profile hot swap (pointer exchange only; freeing happens elsewhere)
        if (CompiledProfile* p = pending_.exchange(nullptr, std::memory_order_acq_rel)) {
            CompiledProfile* old = active_;
            active_ = p;
            proc.setProfile(active_);
            swaps_.fetch_add(1, std::memory_order_relaxed);
            if (old && !retired_.push(old)) {
                // Retire queue full (control thread not collecting): keep the
                // old profile alive by leaking rather than freeing here.
                Logger::instance().logRt(LogLevel::Warn, "engine", "retire queue full; profile leaked");
            }
        }

        // --- reconnect handling
        if (!connected) {
            const TimestampNs now = nowNs();
            if (now < nextReconnect) continue;
            nextReconnect = now + static_cast<TimestampNs>(cfg_.reconnectIntervalMs) * 1'000'000;
            if (source_->open()) {
                connected = true;
                haveLast = haveSeq = false;
                prevPollT = 0;
                proc.reset();
                nextDeadline = nowNs();
                state_.store(static_cast<int>(EngineState::Running));
                Logger::instance().logRt(LogLevel::Info, "engine", "input device connected");
                TelemetrySample ev{};
                ev.t = nowNs();
                ev.flags = kFlagReconnect;
                emit(ev);
            }
            continue;
        }

        RawSample s;
        const TimestampNs pollT = nowNs();
        const TimestampNs pollGap = prevPollT ? pollT - prevPollT : 0;
        prevPollT = pollT;
        const PollResult r = source_->poll(s);
        if (r == PollResult::Disconnected) {
            connected = false;
            disconnects_.fetch_add(1, std::memory_order_relaxed);
            state_.store(static_cast<int>(EngineState::WaitingForDevice));
            // Safety first: centre everything immediately.
            if (!sink_->submit(neutral)) sinkErrors_.fetch_add(1, std::memory_order_relaxed);
            proc.reset();
            panicSince = 0;
            panicLatched = false;
            Logger::instance().logRt(LogLevel::Warn, "engine", "input device disconnected; output neutralized");
            TelemetrySample ev{};
            ev.t = nowNs();
            ev.flags = kFlagDisconnect;
            emit(ev);
            nextReconnect = nowNs() + static_cast<TimestampNs>(cfg_.reconnectIntervalMs) * 1'000'000;
            continue;
        }

        bool idleTick = false;
        if (r == PollResult::NoChange) {
            if (!(cfg_.tickWhileIdle && haveLast && !bypass() && proc.needsTick())) continue;
            s = last;
            s.t = nowNs();
            idleTick = true;
            idleTicks_.fetch_add(1, std::memory_order_relaxed);
        } else {
            newSamples_.fetch_add(1, std::memory_order_relaxed);
            if (source_->hasDeviceSequence()) {
                if (haveSeq) {
                    const std::uint32_t delta = s.sequence - lastSeq;
                    if (delta > 1 && delta < 0x10000u) missed_.fetch_add(delta - 1, std::memory_order_relaxed);
                }
                haveSeq = true;
                lastSeq = s.sequence;
            }
            last = s;
            haveLast = true;

            // Panic combo: hold to toggle pass-through.
            if (cfg_.panic.enabled && cfg_.panic.buttons != 0) {
                if ((s.pad.buttons & cfg_.panic.buttons) == cfg_.panic.buttons) {
                    if (panicSince == 0) panicSince = s.t;
                    if (!panicLatched && s.t - panicSince >= static_cast<TimestampNs>(cfg_.panic.holdMs) * 1'000'000) {
                        panicLatched = true;
                        setBypass(!bypass());
                        Logger::instance().logRt(LogLevel::Info, "engine", "panic combo toggled pass-through", bypass() ? 1 : 0);
                    }
                } else {
                    panicSince = 0;
                    panicLatched = false;
                }
            }
        }

        const bool bp = bypass();
        ProcessTrace trace;
        const TimestampNs t0 = nowNs();
        RawGamepad out;
        if (bp) {
            out = s.pad;
        } else {
            out = denormalize(proc.process(s, &trace));
        }
        const TimestampNs t1 = nowNs();
        const bool ok = sink_->submit(out);
        const TimestampNs t2 = nowNs();
        if (!ok) sinkErrors_.fetch_add(1, std::memory_order_relaxed);

        TelemetrySample ts;
        ts.t = s.t;
        ts.sequence = s.sequence;
        ts.raw = s.pad;
        ts.out = out;
        ts.processNs = clampU32(t1 - t0);
        ts.submitNs = clampU32(t2 - t1);
        ts.totalNs = clampU32(t2 - s.t);
        ts.pollGapNs = idleTick ? 0 : clampU32(pollGap);
        ts.context = static_cast<std::uint8_t>(bp ? ContextId::Hip : proc.activeContext());
        ts.flags = static_cast<std::uint8_t>((bp ? kFlagBypass : 0) | (trace.flickBypass ? kFlagFlick : 0) |
                                             (idleTick ? kFlagIdleTick : 0) | (ok ? 0 : kFlagSinkError));
        emit(ts);
    }

    sink_->submit(neutral);
    sink_->close();
    source_->close();
    win::restoreCurrentThread();
    state_.store(static_cast<int>(EngineState::Stopped));
}

const char* toString(EngineState s) noexcept {
    switch (s) {
        case EngineState::Stopped: return "stopped";
        case EngineState::WaitingForDevice: return "waiting for device";
        case EngineState::Running: return "running";
        case EngineState::Faulted: return "faulted";
    }
    return "stopped";
}

const char* toString(PollMode m) noexcept {
    switch (m) {
        case PollMode::Spin: return "spin";
        case PollMode::Hybrid: return "hybrid";
        case PollMode::Timer: return "timer";
    }
    return "hybrid";
}

}  // namespace sa
