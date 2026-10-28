#include "app_controller.hpp"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <cmath>

#include "stickyaim/core/analysis.hpp"
#include "stickyaim/core/clock.hpp"
#include "stickyaim/core/config_io.hpp"
#include "stickyaim/core/logger.hpp"
#include "stickyaim/core/signals.hpp"
#include "stickyaim/platform/vigem_sink.hpp"
#include "stickyaim/platform/win_util.hpp"
#include "stickyaim/platform/xinput_source.hpp"
#include "i18n.hpp"

namespace {

constexpr std::size_t kRecentMax = 16384;

QString q(const std::string& s) { return QString::fromStdString(s); }
std::string s(const QString& q) { return q.toStdString(); }

// Demo input for the safe preview mode when no controller is connected:
// slow circles, small corrections, flicks and rest with noise, looping.
std::vector<sa::RawSample> demoSignal() {
    std::vector<sa::RawSample> all;
    auto append = [&](sa::SignalSpec spec) {
        spec.rateHz = 1000;
        auto part = sa::generate(spec);
        const sa::TimestampNs base = all.empty() ? 0 : all.back().t + 1'000'000;
        for (auto& p : part) {
            p.t += base;
            all.push_back(p);
        }
    };
    sa::SignalSpec rest;
    rest.kind = sa::SignalSpec::Kind::Rest;
    rest.durationSec = 1.5;
    rest.noiseStd = 0.004f;
    rest.offset = {0.015f, 0.006f};
    append(rest);
    sa::SignalSpec circle;
    circle.kind = sa::SignalSpec::Kind::Circle;
    circle.durationSec = 3;
    circle.amplitude = 0.6f;
    circle.freqHz = 0.5;
    append(circle);
    sa::SignalSpec micro;
    micro.kind = sa::SignalSpec::Kind::MicroAdjust;
    micro.durationSec = 3;
    micro.amplitude = 0.15f;
    micro.freqHz = 0.8;
    micro.noiseStd = 0.002f;
    append(micro);
    sa::SignalSpec flick;
    flick.kind = sa::SignalSpec::Kind::Flick;
    flick.durationSec = 0.6;
    flick.stepAtSec = 0.2;
    flick.angleRad = 3.1416f;
    append(flick);
    sa::SignalSpec full;
    full.kind = sa::SignalSpec::Kind::Circle;
    full.durationSec = 2;
    full.amplitude = 1.0f;
    full.freqHz = 1.0;
    append(full);
    // Hold the left trigger during the micro-adjust part so ADS is visible.
    for (auto& p : all)
        if (p.t > 4'500'000'000ll && p.t < 7'500'000'000ll) p.pad.leftTrigger = 200;
    return all;
}

}  // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
    loadSettings();
    applySettingsToEngineConfig();
    deviceDb_ = sa::loadDeviceDatabase(sa::win::findDataDir("devices", STICKYAIM_SOURCE_DIR));
    rawInput_ = std::make_shared<sa::win::RawInputMonitor>();
    std::string err;
    if (!rawInput_->start(&err)) sa::logWarn("app", "Raw Input monitor failed: " + err);

    // Default source: first connected XInput controller, else the demo signal.
    source_.kind = SourceKind::Demo;
    for (const auto& slot : sa::win::XInputSource::querySlots())
        if (slot.connected) {
            source_.kind = SourceKind::XInput;
            source_.slot = slot.slot;
            break;
        }

    // Initial profile: last used, else the shipped baseline, else defaults.
    const QString last = q(settings_.value("last_profile", std::string()));
    bool loaded = false;
    if (!last.isEmpty() && QFileInfo::exists(last)) loaded = loadProfileFile(last, nullptr);
    if (!loaded) {
        for (const auto& p : profileLibrary())
            if (p.contains("baseline")) {
                loaded = loadProfileFile(p, nullptr);
                break;
            }
    }
    if (!loaded) setProfile(sa::Profile{});

    sa::DeviceCalibration cal;
    if (sa::loadCalibration(s(userDataDir() + "/calibration.json"), cal)) calibration_ = cal;

    publishTimer_.setSingleShot(true);
    publishTimer_.setInterval(25);
    connect(&publishTimer_, &QTimer::timeout, this, &AppController::publishCompiled);
    // One UI update per monitor frame (60, 144, 240 Hz ...): drawing faster is invisible.
    const QScreen* screen = QGuiApplication::primaryScreen();
    const double refreshHz = screen && screen->refreshRate() > 1 ? screen->refreshRate() : 60.0;
    drainTimer_.setTimerType(Qt::PreciseTimer);
    drainTimer_.setInterval(std::max(4, static_cast<int>(1000.0 / refreshHz)));
    connect(&drainTimer_, &QTimer::timeout, this, &AppController::drainTelemetry);
    drainTimer_.start();
}

AppController::~AppController() {
    stopEngine();
    if (rawInput_) rawInput_->stop();
    saveSettings();
}

// ------------------------------------------------------------------ settings

void AppController::loadSettings() {
    std::string text;
    const std::string path = sa::win::userDataDir() + "\\settings.json";
    if (sa::readTextFile(path, text)) {
        auto j = nlohmann::json::parse(text, nullptr, false);
        if (j.is_object()) settings_ = j;
    }
    if (!settings_.is_object()) settings_ = nlohmann::json::object();
}

void AppController::saveSettings() {
    sa::writeTextFile(sa::win::userDataDir() + "\\settings.json", settings_.dump(2));
}

void AppController::resetAllSettings() {
    settings_ = nlohmann::json::object();
    applySettingsToEngineConfig();
    saveSettings();
    if (output_ != OutputKind::Preview) setOutput(OutputKind::Preview, nullptr);
    restartIfRunning();
}

bool AppController::allowVirtualOutput() const { return settings_.value("allow_virtual_output", false); }

void AppController::applySettingsToEngineConfig() {
    sa::EngineConfig c;
    const std::string mode = settings_.value("poll_mode", "hybrid");
    c.mode = mode == "spin" ? sa::PollMode::Spin : mode == "timer" ? sa::PollMode::Timer : sa::PollMode::Hybrid;
    c.intervalUs = settings_.value("interval_us", 125);
    c.useMmcss = settings_.value("mmcss", true);
    c.panic.enabled = settings_.value("panic_enabled", true);
    c.panic.holdMs = settings_.value("panic_hold_ms", 1500);
    engineCfg_ = c;
}

void AppController::setEngineConfig(const sa::EngineConfig& c) {
    engineCfg_ = c;
    settings_["poll_mode"] = sa::toString(c.mode);
    settings_["interval_us"] = c.intervalUs;
    settings_["mmcss"] = c.useMmcss;
    settings_["panic_enabled"] = c.panic.enabled;
    settings_["panic_hold_ms"] = c.panic.holdMs;
    saveSettings();
    restartIfRunning();
}

QString AppController::userDataDir() const { return q(sa::win::userDataDir()); }

QString AppController::userProfilesDir() const {
    const QString d = userDataDir() + "/profiles";
    QDir().mkpath(d);
    return d;
}

QStringList AppController::profileLibrary() const {
    QStringList out;
    for (const QString& dir : {q(sa::win::findDataDir("profiles", STICKYAIM_SOURCE_DIR)), userProfilesDir()}) {
        if (dir.isEmpty()) continue;
        for (const auto& fi : QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) out << fi.absoluteFilePath();
    }
    return out;
}

// ------------------------------------------------------------------ profile

void AppController::setProfile(const sa::Profile& p) {
    profile_ = p;
    dirty_ = true;
    errors_.clear();
    for (const auto& e : sa::validate(p)) errors_ << q(e);
    publishTimer_.start();
    Q_EMIT profileChanged();
}

void AppController::publishCompiled() {
    if (!engine_) return;
    std::vector<std::string> errs;
    auto cp = sa::CompiledProfile::compile(profile_, calibration_, &errs);
    if (!cp) {
        Q_EMIT statusMessage(T("Profile not applied: ") + q(errs.front()));
        return;
    }
    measureSmoothingLag(*cp);
    engine_->publishProfile(std::move(cp));
}

// Only smoothing delays the output (deadzones, curves and sensitivity act on
// the same sample), so the profile's added lag is the cross-correlation delay
// of the synthetic 1 Hz tracking test, the same one the Testing page reports.
void AppController::measureSmoothingLag(const sa::CompiledProfile& cp) {
    sa::SignalSpec track;
    track.kind = sa::SignalSpec::Kind::Sine;
    track.rateHz = 1000.0;
    track.durationSec = 3.0;
    track.amplitude = 0.35f;
    track.freqHz = 1.0;
    auto lag = [&](sa::ContextId ctx) {
        const double ms = sa::measureSignal(cp, ctx, "tracking_1hz", "", track).addedLagMs;
        return std::isfinite(ms) ? std::max(0.0, ms) : 0.0;
    };
    response_.smoothingHipMs = lag(sa::ContextId::Hip);
    response_.smoothingAdsMs = lag(sa::ContextId::Ads);
}

void AppController::resetResponseWindow() {
    seqCount_ = 0;
    addedSumUs_ = 0;
    addedCount_ = 0;
    addedWorstHist_.reset();
}

void AppController::setCalibration(const sa::DeviceCalibration& c) {
    calibration_ = c;
    sa::saveCalibration(c, s(userDataDir() + "/calibration.json"));
    publishTimer_.start();
    Q_EMIT calibrationChanged();
}

bool AppController::loadProfileFile(const QString& path, QString* error) {
    sa::Profile p;
    std::string err;
    if (!sa::loadProfile(s(path), p, &err)) {
        if (error) *error = q(err);
        return false;
    }
    setProfile(p);
    profilePath_ = path;
    dirty_ = false;
    settings_["last_profile"] = s(path);
    saveSettings();
    Q_EMIT profileChanged();
    return true;
}

bool AppController::saveProfileFile(const QString& path, QString* error) {
    std::string err;
    if (!sa::saveProfile(profile_, s(path), &err)) {
        if (error) *error = q(err);
        return false;
    }
    profilePath_ = path;
    dirty_ = false;
    settings_["last_profile"] = s(path);
    saveSettings();
    Q_EMIT profileChanged();
    return true;
}

// ------------------------------------------------------------------ engine

std::unique_ptr<sa::IInputSource> AppController::makeSource() const {
    switch (source_.kind) {
        case SourceKind::XInput: return std::make_unique<sa::win::XInputSource>(source_.slot);
        case SourceKind::Hid: {
            sa::HidMapping mapping = sa::genericGamepadMapping();
            for (const auto& d : sa::win::enumerateHidDevices(true)) {
                if (q(d.path) != source_.hidPath) continue;
                if (const auto* k = sa::matchDevice(deviceDb_, d.vendorId, d.productId); k && k->hasHidMapping) mapping = k->hidMapping;
                else if (d.xinputHidInterface) mapping = sa::defaultXInputHidMapping();
            }
            return std::make_unique<sa::win::HidSource>(rawInput_, s(source_.hidPath), mapping, s(source_.label));
        }
        case SourceKind::Demo:
        default: return std::make_unique<sa::ReplaySource>(demoSignal(), true, "Demo signal (no hardware)");
    }
}

bool AppController::startEngine(QString* error) {
    stopEngine();
    std::unique_ptr<sa::IOutputSink> sink;
    if (output_ == OutputKind::ViGEm) {
        const int physical = source_.kind == SourceKind::XInput ? source_.slot : -1;
        sink = std::make_unique<sa::win::ViGEmX360Sink>(physical);
    } else {
        sink = std::make_unique<sa::PreviewSink>();
    }
    engine_ = std::make_unique<sa::Engine>(makeSource(), std::move(sink), engineCfg_);
    std::vector<std::string> errs;
    if (auto cp = sa::CompiledProfile::compile(profile_, calibration_, &errs)) {
        measureSmoothingLag(*cp);
        engine_->publishProfile(std::move(cp));
    }
    std::string err;
    if (!engine_->start(err)) {
        engine_.reset();
        if (output_ == OutputKind::ViGEm) output_ = OutputKind::Preview;
        if (error) *error = q(err);
        Q_EMIT statusMessage(T("Engine failed to start: ") + q(err));
        Q_EMIT engineStateChanged();
        return false;
    }
    resetLatencyStats();
    recent_.clear();
    haveSeq_ = false;
    resetResponseWindow();
    rateWindowStart_ = 0;
    rateCount_ = 0;
    uiFrames_ = 0;
    uiWindowStart_ = 0;
    uiWorstGapMs_ = ageSumMs_ = 0;
    refreshDeviceMatch();
    const double smoothHip = response_.smoothingHipMs, smoothAds = response_.smoothingAdsMs;
    response_ = {};
    response_.smoothingHipMs = smoothHip;
    response_.smoothingAdsMs = smoothAds;
    sa::logInfo("app", "engine started: " + engine_->source().name() + " -> " + engine_->sink().name());
    Q_EMIT engineStateChanged();
    return true;
}

void AppController::stopEngine() {
    if (!engine_) return;
    engine_->stop();
    drainTelemetry();
    engine_.reset();
    sa::logInfo("app", "engine stopped");
    Q_EMIT engineStateChanged();
}

void AppController::restartIfRunning() {
    if (engine_) startEngine(nullptr);
}

bool AppController::engineRunning() const { return engine_ && engine_->running(); }

void AppController::setSource(const SourceSel& sel) {
    source_ = sel;
    restartIfRunning();
    Q_EMIT engineStateChanged();
}

bool AppController::setOutput(OutputKind kind, QString* error) {
    if (kind == OutputKind::ViGEm && !allowVirtualOutput()) {
        if (error) *error = T("Virtual output is disabled. Enable it in Settings after reading the warnings.");
        return false;
    }
    const OutputKind prev = output_;
    output_ = kind;
    if (engine_ || kind == OutputKind::ViGEm) {
        QString err;
        if (!startEngine(&err)) {
            output_ = prev == OutputKind::ViGEm ? OutputKind::Preview : prev;
            startEngine(nullptr);
            if (error) *error = err;
            return false;
        }
    }
    Q_EMIT engineStateChanged();
    return true;
}

void AppController::setBypass(bool on) {
    if (engine_) engine_->setBypass(on);
    Q_EMIT engineStateChanged();
}

bool AppController::bypass() const { return engine_ && engine_->bypass(); }

sa::EngineCounters AppController::counters() const { return engine_ ? engine_->counters() : sa::EngineCounters{}; }

QString AppController::sourceDescription() const {
    switch (source_.kind) {
        case SourceKind::XInput: return QString(T("XInput slot %1")).arg(source_.slot);
        case SourceKind::Hid: return T("HID: ") + source_.label;
        default: return T("Demo signal (no hardware)");
    }
}

QString AppController::matchedDeviceName() const { return matchedName_; }

// Enumerating HID devices opens every device and reads its strings (~60 ms),
// so it runs only when the source or the connected devices change, never per frame.
void AppController::refreshDeviceMatch() {
    // XInput does not expose VID/PID per slot through public APIs; report the
    // known controllers that are present as XInput HID views.
    QStringList names;
    for (const auto& d : sa::win::enumerateHidDevices(true)) {
        if (!d.xinputHidInterface && source_.kind == SourceKind::XInput) continue;
        if (source_.kind == SourceKind::Hid && q(d.path) != source_.hidPath) continue;
        if (const auto* k = sa::matchDevice(deviceDb_, d.vendorId, d.productId))
            names << q(k->name);
        else if (source_.kind != SourceKind::Demo)
            names << QString(T("VID_%1 PID_%2 %3")).arg(q(sa::hex4(d.vendorId)), q(sa::hex4(d.productId)), q(d.product));
    }
    names.removeDuplicates();
    matchedName_ = names.join(", ");
}

// ------------------------------------------------------------------ telemetry

void AppController::drainTelemetry() {
    if (!engine_) return;
    sa::TelemetrySample t;
    const sa::TimestampNs now = sa::nowNs();
    while (engine_->telemetry().pop(t)) {
        if (t.flags & (sa::kFlagDisconnect | sa::kFlagReconnect)) {
            refreshDeviceMatch();
            Q_EMIT statusMessage((t.flags & sa::kFlagDisconnect) ? T("Controller disconnected: output neutralized")
                                                                  : T("Controller connected"));
            Q_EMIT engineStateChanged();
            continue;
        }
        recent_.push_back(t);
        if (recent_.size() > kRecentMax) recent_.pop_front();
        if (t.flags & sa::kFlagIdleTick) continue;
        // Start the rate window at the first sample, so a backlog queued before
        // the UI was up does not inflate the first reading.
        if (rateWindowStart_ == 0) rateWindowStart_ = t.t;
        sampleTiming_.add(t.t, t.sequence, false);
        processHist_.add(t.processNs / 1000.0);
        submitHist_.add(t.submitNs / 1000.0);
        totalHist_.add(t.totalNs / 1000.0);
        ++rateCount_;
        if (haveSeq_) {
            const std::uint32_t delta = t.sequence - lastSeq_;
            if (delta < 0x10000u) seqCount_ += delta;
        }
        haveSeq_ = true;
        lastSeq_ = t.sequence;
        if (t.pollGapNs) {
            // The state appeared at an unknown moment inside the poll gap:
            // on average half of it, at worst all of it, before the app read it.
            addedSumUs_ += (t.pollGapNs / 2.0 + t.totalNs) / 1000.0;
            ++addedCount_;
            addedWorstHist_.add((static_cast<double>(t.pollGapNs) + t.totalNs) / 1000.0);
        }
        if (capturing_) capture_.push_back({t.t, t.sequence, t.raw});
    }
    if (uiWindowStart_ == 0) uiWindowStart_ = now;
    ++uiFrames_;
    if (lastDrain_) uiWorstGapMs_ = std::max(uiWorstGapMs_, static_cast<double>(now - lastDrain_) * 1e-6);
    lastDrain_ = now;
    if (!recent_.empty()) ageSumMs_ += static_cast<double>(now - recent_.back().t) * 1e-6;
    if (rateWindowStart_ != 0 && now - rateWindowStart_ >= 500'000'000) {
        const double windowSec = static_cast<double>(now - rateWindowStart_) * 1e-9;
        rateHz_ = static_cast<double>(rateCount_) / windowSec;
        response_.appHz = rateHz_;
        response_.deviceRateValid = engine_->source().hasDeviceSequence();
        response_.deviceHz = response_.deviceRateValid ? static_cast<double>(seqCount_) / windowSec : rateHz_;
        response_.deviceIntervalMs = response_.deviceHz > 0 ? 1000.0 / response_.deviceHz : 0;
        response_.addedMeanMs = addedCount_ ? addedSumUs_ / addedCount_ / 1000.0 : 0;
        response_.addedP99Ms = addedWorstHist_.percentile(99) / 1000.0;
        // Frame rate from the UI updates seen in this window (intervals between the first and this one).
        const double uiSec = static_cast<double>(now - uiWindowStart_) * 1e-9;
        response_.uiFps = uiFrames_ > 1 && uiSec > 0 ? static_cast<double>(uiFrames_ - 1) / uiSec : 0;
        response_.uiWorstFrameMs = uiWorstGapMs_;
        response_.displayAgeMs = uiFrames_ ? ageSumMs_ / static_cast<double>(uiFrames_) : 0;
        uiFrames_ = 0;
        uiWindowStart_ = 0;
        uiWorstGapMs_ = ageSumMs_ = 0;
        if (now - lastUiLog_ >= 5'000'000'000) {
            lastUiLog_ = now;
            sa::logInfo("ui", "fps " + std::to_string(response_.uiFps) + " worst_frame_ms " +
                                  std::to_string(response_.uiWorstFrameMs) + " display_age_ms " +
                                  std::to_string(response_.displayAgeMs) + " device_hz " + std::to_string(response_.deviceHz));
        }
        resetResponseWindow();
        rateCount_ = 0;
        rateWindowStart_ = now;
    }
    Q_EMIT telemetryTick();
}

void AppController::resetLatencyStats() {
    sampleTiming_.reset();
    processHist_.reset();
    submitHist_.reset();
    totalHist_.reset();
    if (rawInput_) rawInput_->resetStats();
}

void AppController::beginCapture() {
    capture_.clear();
    capture_.reserve(200000);
    capturing_ = true;
}

std::vector<sa::RawSample> AppController::endCapture() {
    capturing_ = false;
    return std::move(capture_);
}

double AppController::captureSeconds() const {
    if (capture_.size() < 2) return 0;
    return static_cast<double>(capture_.back().t - capture_.front().t) * 1e-9;
}
