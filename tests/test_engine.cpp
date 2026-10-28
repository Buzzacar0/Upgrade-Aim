#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "stickyaim/core/clock.hpp"
#include "stickyaim/engine/engine.hpp"

using namespace sa;
using namespace std::chrono_literals;

namespace {

// Scripted input: hands out queued samples; can be told to disappear and come back.
class FakeSource final : public IInputSource {
public:
    struct Shared {
        std::mutex mu;
        std::deque<RawSample> queue;
        bool present = true;
        int opens = 0;
    };
    explicit FakeSource(std::shared_ptr<Shared> sh) : sh_(std::move(sh)) {}
    std::string name() const override { return "fake"; }
    bool open() override {
        std::lock_guard lk(sh_->mu);
        ++sh_->opens;
        return sh_->present;
    }
    void close() override {}
    PollResult poll(RawSample& out) noexcept override {
        std::lock_guard lk(sh_->mu);
        if (!sh_->present) return PollResult::Disconnected;
        if (sh_->queue.empty()) return PollResult::NoChange;
        out = sh_->queue.front();
        out.t = nowNs();
        sh_->queue.pop_front();
        return PollResult::NewSample;
    }
    bool hasDeviceSequence() const noexcept override { return true; }

private:
    std::shared_ptr<Shared> sh_;
};

class RecordingSink final : public IOutputSink {
public:
    struct Shared {
        std::mutex mu;
        std::vector<RawGamepad> submitted;
        bool open = false;
        bool closed = false;
    };
    explicit RecordingSink(std::shared_ptr<Shared> sh) : sh_(std::move(sh)) {}
    std::string name() const override { return "recording"; }
    bool open(std::string&) override {
        std::lock_guard lk(sh_->mu);
        sh_->open = true;
        return true;
    }
    void close() noexcept override {
        std::lock_guard lk(sh_->mu);
        sh_->closed = true;
    }
    bool submit(const RawGamepad& p) noexcept override {
        std::lock_guard lk(sh_->mu);
        sh_->submitted.push_back(p);
        return true;
    }
    bool injectsIntoSystem() const noexcept override { return false; }

private:
    std::shared_ptr<Shared> sh_;
};

struct Rig {
    std::shared_ptr<FakeSource::Shared> src = std::make_shared<FakeSource::Shared>();
    std::shared_ptr<RecordingSink::Shared> sink = std::make_shared<RecordingSink::Shared>();
    std::unique_ptr<Engine> engine;

    explicit Rig(EngineConfig cfg = fastConfig()) {
        engine = std::make_unique<Engine>(std::make_unique<FakeSource>(src), std::make_unique<RecordingSink>(sink), cfg);
    }
    static EngineConfig fastConfig() {
        EngineConfig c;
        c.mode = PollMode::Hybrid;
        c.intervalUs = 200;
        c.useMmcss = false;
        c.reconnectIntervalMs = 20;
        return c;
    }
    void push(std::int16_t rx, std::int16_t ry, std::uint16_t buttons = 0, std::uint32_t seq = 0) {
        std::lock_guard lk(src->mu);
        RawSample s;
        s.pad.rx = rx;
        s.pad.ry = ry;
        s.pad.buttons = buttons;
        s.sequence = seq;
        src->queue.push_back(s);
    }
    void setPresent(bool p) {
        std::lock_guard lk(src->mu);
        src->present = p;
    }
    std::vector<RawGamepad> submitted() {
        std::lock_guard lk(sink->mu);
        return sink->submitted;
    }
    bool waitFor(const std::function<bool()>& pred, std::chrono::milliseconds timeout = 2000ms) {
        const auto end = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < end) {
            if (pred()) return true;
            std::this_thread::sleep_for(1ms);
        }
        return pred();
    }
    void drainQueueEmpty() {
        waitFor([&] {
            std::lock_guard lk(src->mu);
            return src->queue.empty();
        });
    }
};

std::unique_ptr<CompiledProfile> squaredProfile() {
    Profile p;
    p.hip.deadzone = {DeadzoneType::Radial, 0.0f, 1.0f};
    p.hip.radial.curve = {CurveType::Power, 2.0f};
    p.ads = p.hip;
    return CompiledProfile::compile(p, {});
}

}  // namespace

TEST(Engine, ProcessesEachSampleThroughProfile) {
    Rig rig;
    rig.engine->publishProfile(squaredProfile());
    std::string err;
    ASSERT_TRUE(rig.engine->start(err)) << err;
    // Give the engine a moment to pick up the profile before sending input.
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->counters().profileSwaps == 1; }));
    rig.push(16384, 0, 0, 1);
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return !rig.submitted().empty(); }));
    rig.engine->stop();
    const auto subs = rig.submitted();
    ASSERT_GE(subs.size(), 2u);  // processed sample + neutral on stop
    EXPECT_NEAR(subs.front().rx, 8192, 8);  // 0.5^2 = 0.25 of full scale
    EXPECT_EQ(subs.back(), RawGamepad{});    // neutral on shutdown
    std::lock_guard lk(rig.sink->mu);
    EXPECT_TRUE(rig.sink->closed);
}

TEST(Engine, BypassPassesRawInputUnchanged) {
    Rig rig;
    rig.engine->publishProfile(squaredProfile());
    rig.engine->setBypass(true);
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    rig.push(16384, -1000, Button::A, 1);
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return !rig.submitted().empty(); }));
    rig.engine->stop();
    const auto first = rig.submitted().front();
    EXPECT_EQ(first.rx, 16384);
    EXPECT_EQ(first.ry, -1000);
    EXPECT_EQ(first.buttons, Button::A);
}

TEST(Engine, DisconnectNeutralizesOutputAndReconnects) {
    Rig rig;
    rig.engine->publishProfile(squaredProfile());
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    rig.push(32767, 32767, Button::RightShoulder, 1);
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return !rig.submitted().empty(); }));

    rig.setPresent(false);
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->counters().disconnects == 1; }));
    EXPECT_EQ(rig.engine->counters().state, EngineState::WaitingForDevice);
    {
        const auto subs = rig.submitted();
        EXPECT_EQ(subs.back(), RawGamepad{}) << "output must be centred and released on disconnect";
    }

    rig.setPresent(true);
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->counters().state == EngineState::Running; }));
    const auto before = rig.submitted().size();
    rig.push(-16384, 0, 0, 2);
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return rig.submitted().size() > before; }));
    rig.engine->stop();
    EXPECT_NEAR(rig.submitted()[before].rx, -8192, 8);

    // Telemetry shows the disconnect and reconnect events.
    bool sawDisconnect = false, sawReconnect = false;
    TelemetrySample t;
    while (rig.engine->telemetry().pop(t)) {
        sawDisconnect |= (t.flags & kFlagDisconnect) != 0;
        sawReconnect |= (t.flags & kFlagReconnect) != 0;
    }
    EXPECT_TRUE(sawDisconnect);
    EXPECT_TRUE(sawReconnect);
}

TEST(Engine, StartsWithoutDeviceAndWaits) {
    Rig rig;
    rig.setPresent(false);
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->counters().state == EngineState::WaitingForDevice; }));
    rig.setPresent(true);
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->counters().state == EngineState::Running; }));
    rig.engine->stop();
    EXPECT_EQ(rig.engine->counters().state, EngineState::Stopped);
}

TEST(Engine, PanicComboTogglesPassThrough) {
    EngineConfig cfg = Rig::fastConfig();
    cfg.panic.holdMs = 30;
    Rig rig(cfg);
    rig.engine->publishProfile(squaredProfile());
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    const std::uint16_t combo = Button::Back | Button::Start;
    // Hold the combo across several samples spanning > 30 ms.
    for (std::uint32_t i = 0; i < 6; ++i) {
        rig.push(0, 0, combo, i + 1);
        std::this_thread::sleep_for(10ms);
    }
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->bypass(); }));
    // Releasing and holding again toggles it back.
    rig.push(0, 0, 0, 7);
    for (std::uint32_t i = 0; i < 6; ++i) {
        rig.push(0, 0, combo, 8 + i);
        std::this_thread::sleep_for(10ms);
    }
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return !rig.engine->bypass(); }));
    rig.engine->stop();
}

TEST(Engine, CountsMissedDevicePackets) {
    Rig rig;
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    rig.push(100, 0, 0, 10);
    rig.push(200, 0, 0, 11);
    rig.push(300, 0, 0, 15);  // 12, 13, 14 never seen
    rig.drainQueueEmpty();
    ASSERT_TRUE(rig.waitFor([&] { return rig.engine->counters().newSamples == 3; }));
    rig.engine->stop();
    EXPECT_EQ(rig.engine->counters().missedSequence, 3u);
}

TEST(Engine, ProfileHotSwapUnderLoadIsSafe) {
    Rig rig;
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    std::atomic<bool> stop{false};
    std::thread feeder([&] {
        std::uint32_t seq = 1;
        while (!stop) {
            rig.push(static_cast<std::int16_t>(seq * 37 % 32767), 0, 0, seq);
            ++seq;
            std::this_thread::sleep_for(200us);
        }
    });
    for (int i = 0; i < 300; ++i) {
        Profile p;
        p.hip.radial.curve = {CurveType::Power, 1.0f + (i % 10) * 0.1f};
        p.ads = p.hip;
        rig.engine->publishProfile(CompiledProfile::compile(p, {}));
        std::this_thread::sleep_for(1ms);
    }
    stop = true;
    feeder.join();
    rig.engine->stop();
    const auto c = rig.engine->counters();
    EXPECT_GT(c.profileSwaps, 10u);
    EXPECT_LE(c.profileSwaps, 300u);
    EXPECT_EQ(c.sinkErrors, 0u);
}

TEST(Engine, TelemetryReportsTimings) {
    Rig rig;
    rig.engine->publishProfile(squaredProfile());
    std::string err;
    ASSERT_TRUE(rig.engine->start(err));
    for (std::uint32_t i = 1; i <= 50; ++i) rig.push(static_cast<std::int16_t>(i * 500), 0, 0, i);
    rig.drainQueueEmpty();
    rig.engine->stop();
    TelemetrySample t;
    int n = 0;
    while (rig.engine->telemetry().pop(t)) {
        if (t.flags & kFlagIdleTick) continue;
        ++n;
        EXPECT_LE(t.processNs, t.totalNs + 1);
        EXPECT_LT(t.totalNs, 50'000'000u);  // < 50 ms even on a busy CI machine
    }
    EXPECT_EQ(n, 50);
}

TEST(Engine, ReplaySourceDeliversInRealTime) {
    std::vector<RawSample> samples(100);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i].t = static_cast<TimestampNs>(i) * 1'000'000;  // 1 kHz
        samples[i].pad.rx = static_cast<std::int16_t>(i * 100);
    }
    auto sink = std::make_shared<RecordingSink::Shared>();
    EngineConfig cfg = Rig::fastConfig();
    Engine e(std::make_unique<ReplaySource>(samples, false), std::make_unique<RecordingSink>(sink), cfg);
    std::string err;
    ASSERT_TRUE(e.start(err));
    std::this_thread::sleep_for(250ms);
    e.stop();
    const auto c = e.counters();
    // 100 samples over 100 ms; at a 200 us poll period essentially all are seen.
    EXPECT_GE(c.newSamples + c.missedSequence, 99u);
    EXPECT_GE(c.newSamples, 80u);
}
