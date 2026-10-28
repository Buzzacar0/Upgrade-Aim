// upgradeaim-cli — command-line diagnostics for Upgrade Aim.
//
// Every number printed here is measured on this machine at run time. Nothing
// is assumed from device specifications.

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "stickyaim/core/analysis.hpp"
#include "stickyaim/core/calibration.hpp"
#include "stickyaim/core/clock.hpp"
#include "stickyaim/core/config_io.hpp"
#include "stickyaim/core/device_db.hpp"
#include "stickyaim/core/logger.hpp"
#include "stickyaim/core/recording.hpp"
#include "stickyaim/core/stats.hpp"
#include "stickyaim/engine/engine.hpp"
#include "stickyaim/platform/rawinput.hpp"
#include "stickyaim/platform/system_probe.hpp"
#include "stickyaim/platform/vigem_sink.hpp"
#include "stickyaim/platform/win_util.hpp"
#include "stickyaim/platform/xinput_source.hpp"

using namespace sa;

namespace {

volatile bool g_interrupted = false;

BOOL WINAPI ctrlHandler(DWORD) {
    g_interrupted = true;
    return TRUE;
}

struct Args {
    std::string command;
    std::map<std::string, std::string> opts;
    bool has(const std::string& k) const { return opts.count(k) > 0; }
    std::string get(const std::string& k, const std::string& def = {}) const {
        auto it = opts.find(k);
        return it == opts.end() ? def : it->second;
    }
    double num(const std::string& k, double def) const {
        auto it = opts.find(k);
        return it == opts.end() ? def : std::stod(it->second);
    }
};

Args parse(int argc, char** argv) {
    Args a;
    if (argc > 1) a.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string k = argv[i];
        if (k.rfind("--", 0) != 0) continue;
        k = k.substr(2);
        if (i + 1 < argc && std::strncmp(argv[i + 1], "--", 2) != 0)
            a.opts[k] = argv[++i];
        else
            a.opts[k] = "1";
    }
    return a;
}

void usage() {
    std::puts(
        "Upgrade Aim CLI\n"
        "\n"
        "  devices                         controllers (XInput + HID), known-device matches, launchers, drivers\n"
        "  measure  [--slot N] [--seconds S] [--mode spin|hybrid|timer] [--interval US]\n"
        "                                  measure XInput state-change timing and Raw Input HID report timing.\n"
        "                                  Move the right stick continuously in circles while it runs.\n"
        "  calibrate [--slot N] [--rest S] [--range S] [--out FILE]\n"
        "                                  rest analysis (noise, drift, deadzone recommendation) + range sweep\n"
        "  record   [--slot N] [--seconds S] --out FILE.csv\n"
        "  analyze  --profile FILE --in FILE.csv           replay a recording through a profile\n"
        "  compare  --profile FILE [--context hip|ads] [--rate HZ] [--out FILE.json]\n"
        "                                  repeatable synthetic raw-vs-processed comparison\n"
        "  bench    --profile FILE [--samples N]           processing cost on this machine\n"
        "  run      --profile FILE [--slot N] [--seconds S] [--output preview|vigem] [--confirm-injection]\n"
        "                                  run the engine; preview never injects. vigem requires --confirm-injection.\n"
        "\n"
        "Profiles: see the 'profiles' folder. Logs: %APPDATA%\\UpgradeAim\\logs.\n");
}

std::string devicesDir() { return win::findDataDir("devices", STICKYAIM_SOURCE_DIR); }

bool loadProfileArg(const Args& a, Profile& p) {
    if (!a.has("profile")) {
        std::fprintf(stderr, "--profile is required\n");
        return false;
    }
    std::string err;
    if (!loadProfile(a.get("profile"), p, &err)) {
        std::fprintf(stderr, "cannot load profile: %s\n", err.c_str());
        return false;
    }
    return true;
}

int firstConnectedSlot() {
    for (const auto& s : win::XInputSource::querySlots())
        if (s.connected) return s.slot;
    return -1;
}

int resolveSlot(const Args& a) {
    if (a.has("slot")) return static_cast<int>(a.num("slot", 0));
    const int s = firstConnectedSlot();
    if (s < 0) std::fprintf(stderr, "no XInput controller connected\n");
    return s;
}

void printTiming(const char* label, const TimingSummary& t) {
    std::printf("  %-34s n=%-7llu mean=%8.1f us  (%.0f Hz)  sd=%7.1f  p50=%7.1f  p95=%7.1f  p99=%7.1f  max=%8.1f\n", label,
                static_cast<unsigned long long>(t.samples), t.meanUs, t.effectiveRateHz, t.stddevUs, t.p50Us, t.p95Us,
                t.p99Us, t.maxUs);
    if (t.gaps || t.missedSequence)
        std::printf("  %-34s gaps(>%.0f us)=%llu  missed device packets=%llu\n", "", t.gapThresholdUs,
                    static_cast<unsigned long long>(t.gaps), static_cast<unsigned long long>(t.missedSequence));
}

// ------------------------------------------------------------------ devices

int cmdDevices() {
    std::vector<std::string> dbErrors;
    const auto db = loadDeviceDatabase(devicesDir(), &dbErrors);

    std::puts("XInput slots:");
    for (const auto& s : win::XInputSource::querySlots()) {
        if (s.connected)
            std::printf("  slot %d: connected  subtype=%s  wireless=%s\n", s.slot, s.subtype.c_str(), s.wireless ? "yes" : "no");
        else
            std::printf("  slot %d: -\n", s.slot);
    }

    std::puts("\nHID game controllers and their sibling interfaces (Raw Input):");
    for (const auto& d : win::enumerateHidDevices(true)) {
        const DeviceMode* mode = nullptr;
        const KnownDevice* k = matchDevice(db, d.vendorId, d.productId, &mode);
        std::printf("  VID_%s PID_%s  MI=%2d  %-28s %s%s\n", hex4(d.vendorId).c_str(), hex4(d.productId).c_str(),
                    d.interfaceNumber, d.usageName().c_str(), d.product.empty() ? "(no product string)" : d.product.c_str(),
                    d.xinputHidInterface ? "  [XInput HID view]" : "");
        if (k) std::printf("      known device: %s, mode: %s%s\n", k->name.c_str(), mode->mode.c_str(),
                           mode->verification.empty() ? " (unverified)" : "");
    }

    std::puts("\nLaunchers:");
    for (const auto& l : win::probeLaunchers())
        std::printf("  %-26s installed=%-3s running=%-3s  [%s]\n", l.name.c_str(), l.installed ? "yes" : "no",
                    l.running ? "yes" : "no", l.detection.c_str());

    const auto drv = win::probeDrivers();
    std::puts("\nDrivers / services:");
    std::printf("  ViGEmBus : %s\n  HidHide  : %s\n  xusb22   : %s\n", drv.vigemBus.state.c_str(), drv.hidHide.state.c_str(),
                drv.xusb.state.c_str());
    std::printf("  ViGEm client compiled in: %s\n  Process elevated: %s\n", win::ViGEmX360Sink::isCompiledIn() ? "yes" : "no",
                drv.isElevated ? "yes" : "no");
    for (const auto& e : dbErrors) std::fprintf(stderr, "device db: %s\n", e.c_str());
    return 0;
}

// ------------------------------------------------------------------ measure

int cmdMeasure(const Args& a) {
    const int slot = resolveSlot(a);
    const double seconds = a.num("seconds", 10);
    const std::string mode = a.get("mode", "spin");
    const int intervalUs = static_cast<int>(a.num("interval", 125));

    auto monitor = std::make_shared<win::RawInputMonitor>();
    std::string err;
    if (!monitor->start(&err)) std::fprintf(stderr, "Raw Input monitor unavailable: %s\n", err.c_str());

    win::XInputSource src(slot);
    const bool haveX = slot >= 0 && src.open();
    std::printf("Measuring for %.0f s. Move the RIGHT stick continuously (circles) — unchanged states are invisible to XInput.\n", seconds);
    std::printf("XInput slot %d (%s), poll mode %s%s\n", slot, haveX ? "connected" : "not available", mode.c_str(),
                mode == "spin" ? "" : (" " + std::to_string(intervalUs) + " us").c_str());

    std::string prio = win::elevateCurrentThread(true);
    win::PreciseWaiter waiter;
    TimingAnalyzer changes(3.0, 1.0, 50000);
    TimingAnalyzer pollLoop(3.0, 1.0, 50000);
    Histogram callCost(0.1, 20000);  // XInputGetState duration, 0.1 us bins
    std::uint64_t polls = 0, newSamples = 0;
    const TimestampNs end = nowNs() + static_cast<TimestampNs>(seconds * 1e9);
    TimestampNs deadline = nowNs();
    TimestampNs lastPoll = 0;
    while (nowNs() < end && !g_interrupted) {
        if (mode != "spin") {
            deadline += static_cast<TimestampNs>(intervalUs) * 1000;
            waiter.waitUntil(deadline, mode == "hybrid" ? 150'000 : 0);
        }
        if (!haveX) continue;
        RawSample s;
        const TimestampNs a0 = nowNs();
        const PollResult r = src.poll(s);
        const TimestampNs a1 = nowNs();
        callCost.add(static_cast<double>(a1 - a0) / 1000.0);
        if (lastPoll) pollLoop.add(a0, 0, false);
        lastPoll = a0;
        ++polls;
        if (r == PollResult::NewSample) {
            ++newSamples;
            changes.add(s.t, s.sequence, true);
        } else if (r == PollResult::Disconnected) {
            std::puts("controller disconnected");
            break;
        }
    }
    win::restoreCurrentThread();

    std::printf("\nThread priority: %s\n", prio.c_str());
    if (haveX) {
        std::puts("XInput (host-side timestamps):");
        printTiming("poll loop interval", pollLoop.summary());
        std::printf("  %-34s mean=%.2f us  p99=%.2f us  max=%.2f us\n", "XInputGetState() call cost", callCost.mean(),
                    callCost.percentile(99), callCost.max());
        printTiming("state-change interval (packet#)", changes.summary());
        const auto cs = changes.summary();
        std::printf("  polls=%llu  new states=%llu  packet numbers skipped between polls=%llu\n",
                    static_cast<unsigned long long>(polls), static_cast<unsigned long long>(newSamples),
                    static_cast<unsigned long long>(cs.missedSequence));
        std::puts("  NOTE: packet numbers count driver state changes, not USB reports. Skipped packet numbers mean the");
        std::puts("        driver produced states faster than this loop observed them.");
    }

    std::puts("\nRaw Input HID reports (arrival at this process):");
    bool any = false;
    for (const auto& d : monitor->snapshot()) {
        if (d.reports == 0) continue;
        any = true;
        std::printf("  VID_%s PID_%s %s%s\n", hex4(d.info.vendorId).c_str(), hex4(d.info.productId).c_str(),
                    d.info.usageName().c_str(), d.info.xinputHidInterface ? " [XInput HID view]" : "");
        printTiming("report interval", d.timing);
        std::printf("  %-34s reports=%llu  messages=%llu  multi-report messages=%llu\n", "",
                    static_cast<unsigned long long>(d.reports), static_cast<unsigned long long>(d.messages),
                    static_cast<unsigned long long>(d.coalescedMessages));
    }
    if (!any) std::puts("  no HID reports received from game controllers");
    monitor->stop();
    return 0;
}

// ------------------------------------------------------------------ calibrate

std::vector<RawSample> captureXInput(int slot, double seconds, const char* prompt) {
    std::vector<RawSample> out;
    win::XInputSource src(slot);
    if (!src.open()) return out;
    std::printf("%s (%.0f s)...\n", prompt, seconds);
    const TimestampNs end = nowNs() + static_cast<TimestampNs>(seconds * 1e9);
    TimestampNs lastAny = 0;
    RawSample last{};
    bool have = false;
    while (nowNs() < end && !g_interrupted) {
        RawSample s;
        const PollResult r = src.poll(s);
        if (r == PollResult::NewSample) {
            out.push_back(s);
            last = s;
            have = true;
            lastAny = s.t;
        } else if (r == PollResult::NoChange && have && nowNs() - lastAny > 2'000'000) {
            // XInput reports nothing while the state is unchanged; keep the
            // still state represented (every 2 ms) so rest statistics are not
            // biased toward moments of change.
            last.t = nowNs();
            out.push_back(last);
            lastAny = last.t;
        } else if (r == PollResult::Disconnected) {
            break;
        }
        // Spin-poll: std::this_thread::sleep_for() rounds up to the system
        // timer tick (often 15.6 ms on Windows) and would drop most states.
        YieldProcessor();
    }
    return out;
}

int cmdCalibrate(const Args& a) {
    const int slot = resolveSlot(a);
    if (slot < 0) return 1;
    const auto rest = captureXInput(slot, a.num("rest", 4), "Do NOT touch the sticks");
    RestAnalyzer rl, rr;
    for (const auto& s : rest) {
        rl.add({normalizeAxis(s.pad.lx), normalizeAxis(s.pad.ly)});
        rr.add({normalizeAxis(s.pad.rx), normalizeAxis(s.pad.ry)});
    }
    const auto range = captureXInput(slot, a.num("range", 6), "Now rotate BOTH sticks slowly around the outer edge, several turns");
    RangeAnalyzer gl, gr;
    for (const auto& s : range) {
        gl.add({normalizeAxis(s.pad.lx), normalizeAxis(s.pad.ly)});
        gr.add({normalizeAxis(s.pad.rx), normalizeAxis(s.pad.ry)});
    }

    auto printRest = [](const char* name, const RestReport& r) {
        std::printf("%s stick at rest: n=%zu  mean=(%+.4f, %+.4f)  sd=(%.4f, %.4f)  max|v|=%.4f  max|v-mean|=%.4f  diffRMS=%.5f\n",
                    name, r.samples, r.mean.x, r.mean.y, r.stddev.x, r.stddev.y, r.maxRadiusFromZero, r.maxRadiusFromMean, r.diffRms);
        std::printf("   recommended inner deadzone: %.3f without center calibration, %.3f with it%s%s\n",
                    r.recommendedInnerUncalibrated, r.recommendedInnerCalibrated, r.driftSuspected ? "  [DRIFT SUSPECTED]" : "",
                    r.noisy ? "  [NOISY]" : "");
    };
    auto printRange = [](const char* name, const RangeReport& r) {
        std::printf("%s stick range: x[%+.3f, %+.3f] y[%+.3f, %+.3f]  sectors=%d/%d  weakest=%.3f  circularity err=%.1f%%  "
                    "recommended outer=%.3f\n",
                    name, r.minX, r.maxX, r.minY, r.maxY, r.sectorsCovered, RangeReport::kSectors, r.minSectorMax,
                    r.circularityError * 100, r.recommendedOuter);
    };
    const auto lr = rl.report(), rrr = rr.report();
    const auto glr = gl.report(), grr = gr.report();
    std::puts("");
    printRest("Left ", lr);
    printRest("Right", rrr);
    printRange("Left ", glr);
    printRange("Right", grr);
    if (grr.sectorsCovered < RangeReport::kSectors) std::puts("   (range sweep incomplete: rotate more slowly / fully for a reliable outer value)");

    DeviceCalibration cal;
    cal.deviceKey = "xinput:" + std::to_string(slot);
    cal.left = gl.toCalibration(lr.mean);
    cal.right = gr.toCalibration(rrr.mean);
    if (a.has("out")) {
        std::string err;
        if (!saveCalibration(cal, a.get("out"), &err)) {
            std::fprintf(stderr, "save failed: %s\n", err.c_str());
            return 1;
        }
        std::printf("Calibration written to %s\n", a.get("out").c_str());
    }
    return 0;
}

// ------------------------------------------------------------------ record / analyze

int cmdRecord(const Args& a) {
    const int slot = resolveSlot(a);
    if (slot < 0 || !a.has("out")) {
        std::fprintf(stderr, "usage: record [--slot N] [--seconds S] --out FILE.csv\n");
        return 1;
    }
    auto samples = captureXInput(slot, a.num("seconds", 10), "Recording");
    std::string err;
    if (!saveRecording(samples, a.get("out"), &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::printf("%zu samples written to %s\n", samples.size(), a.get("out").c_str());
    return 0;
}

int cmdAnalyze(const Args& a) {
    Profile p;
    if (!loadProfileArg(a, p)) return 1;
    std::vector<RawSample> rec;
    std::string err;
    if (!loadRecording(a.get("in"), rec, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    DeviceCalibration cal;
    if (a.has("calibration")) loadCalibration(a.get("calibration"), cal);
    auto cp = CompiledProfile::compile(p, cal);
    const auto r = analyzeRecording(*cp, rec, a.get("in"));
    std::puts(toJson(r).dump(2).c_str());
    return 0;
}

// ------------------------------------------------------------------ compare / bench

int cmdCompare(const Args& a) {
    Profile p;
    if (!loadProfileArg(a, p)) return 1;
    std::vector<std::string> errs;
    auto cp = CompiledProfile::compile(p, {}, &errs);
    if (!cp) {
        std::fprintf(stderr, "profile invalid: %s\n", errs.front().c_str());
        return 1;
    }
    const ContextId ctx = a.get("context", "hip") == "ads" ? ContextId::Ads : ContextId::Hip;
    const auto rep = runSyntheticComparison(*cp, ctx, a.num("rate", 1000));
    std::printf("Synthetic comparison: profile '%s', context %s, %.0f Hz (objective signal metrics only)\n\n", rep.profileName.c_str(),
                rep.context.c_str(), rep.rateHz);
    std::printf("%-14s %9s %9s %9s %8s %9s %9s %11s %11s\n", "test", "lag ms", "t50 raw", "t50 proc", "gain", "rest rms", "levels",
                "jerk raw", "jerk proc");
    for (const auto& t : rep.tests) {
        auto f = [](double v) {
            char b[32];
            if (std::isfinite(v)) std::snprintf(b, sizeof(b), "%.3f", v);
            else std::snprintf(b, sizeof(b), "-");
            return std::string(b);
        };
        std::printf("%-14s %9s %9s %9s %8.3f %9s %4d/%-4d %11.2e %11.2e\n", t.name.c_str(), f(t.addedLagMs).c_str(),
                    f(t.rawT50Ms).c_str(), f(t.procT50Ms).c_str(), t.gainRatio, f(t.procRestRms).c_str(), t.rawDistinctLevels,
                    t.procDistinctLevels, t.rawJerkRms, t.procJerkRms);
    }
    std::printf("\nprocessing cost: batch mean %.1f ns/sample (per-sample p99 %.0f ns, timer resolution %.0f ns)\n",
                rep.cost.batchMeanNs, rep.cost.p99Ns, rep.cost.timerResolutionNs);
    if (a.has("out")) {
        std::string err;
        if (!writeTextFile(a.get("out"), toJson(rep).dump(2) + "\n", &err)) std::fprintf(stderr, "%s\n", err.c_str());
        else std::printf("report written to %s\n", a.get("out").c_str());
    }
    return 0;
}

int cmdBench(const Args& a) {
    Profile p;
    if (!loadProfileArg(a, p)) return 1;
    auto cp = CompiledProfile::compile(p, {});
    const auto c = benchmarkProcessing(*cp, static_cast<std::size_t>(a.num("samples", 500000)));
    std::puts(toJson(c).dump(2).c_str());
    return 0;
}

// ------------------------------------------------------------------ run

int cmdRun(const Args& a) {
    Profile p;
    if (!loadProfileArg(a, p)) return 1;
    const int slot = resolveSlot(a);
    if (slot < 0) return 1;
    DeviceCalibration cal;
    if (a.has("calibration")) loadCalibration(a.get("calibration"), cal);
    std::vector<std::string> errs;
    auto cp = CompiledProfile::compile(p, cal, &errs);
    if (!cp) {
        std::fprintf(stderr, "profile invalid: %s\n", errs.front().c_str());
        return 1;
    }

    std::unique_ptr<IOutputSink> sink;
    const std::string output = a.get("output", "preview");
    if (output == "vigem") {
        if (!a.has("confirm-injection")) {
            std::fputs("Refusing to inject without --confirm-injection.\n"
                       "Virtual controller input may violate a game's terms of service and can be detected by anti-cheat\n"
                       "systems (including Call of Duty RICOCHET). See docs/DELIVERY.md before using it.\n",
                       stderr);
            return 2;
        }
        sink = std::make_unique<win::ViGEmX360Sink>(slot);
    } else {
        sink = std::make_unique<PreviewSink>();
    }

    EngineConfig cfg;
    const std::string mode = a.get("mode", "hybrid");
    cfg.mode = mode == "spin" ? PollMode::Spin : mode == "timer" ? PollMode::Timer : PollMode::Hybrid;
    cfg.intervalUs = static_cast<int>(a.num("interval", 125));
    Engine engine(std::make_unique<win::XInputSource>(slot), std::move(sink), cfg);
    engine.publishProfile(std::move(cp));
    std::string err;
    if (!engine.start(err)) {
        std::fprintf(stderr, "cannot start: %s\n", err.c_str());
        return 1;
    }
    std::printf("Engine running: %s -> %s. Hold Back+Start 1.5 s to toggle pass-through. Ctrl+C to stop.\n",
                engine.source().name().c_str(), engine.sink().name().c_str());

    const double seconds = a.num("seconds", 0);
    const auto t0 = std::chrono::steady_clock::now();
    Histogram total(1.0, 20000), proc(0.01, 100000), submit(0.1, 100000);
    while (!g_interrupted) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        TelemetrySample t;
        while (engine.telemetry().pop(t)) {
            if (t.flags & (kFlagDisconnect | kFlagReconnect)) continue;
            total.add(t.totalNs / 1000.0);
            proc.add(t.processNs / 1000.0);
            submit.add(t.submitNs / 1000.0);
        }
        const auto c = engine.counters();
        std::printf("[%s] new=%llu idle=%llu missed=%llu late=%llu disc=%llu bypass=%s | process p50=%.2f p99=%.2f us | "
                    "submit p50=%.1f p99=%.1f us | acquire->submit p99=%.1f us\n",
                    toString(c.state), static_cast<unsigned long long>(c.newSamples), static_cast<unsigned long long>(c.idleTicks),
                    static_cast<unsigned long long>(c.missedSequence), static_cast<unsigned long long>(c.lateWakeups),
                    static_cast<unsigned long long>(c.disconnects), c.bypass ? "ON" : "off", proc.percentile(50),
                    proc.percentile(99), submit.percentile(50), submit.percentile(99), total.percentile(99));
        if (seconds > 0 && std::chrono::steady_clock::now() - t0 >= std::chrono::duration<double>(seconds)) break;
    }
    engine.stop();
    std::puts("stopped; output neutralized.");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
    const Args a = parse(argc, argv);
    const std::string logDir = win::userDataDir() + "\\logs";
    CreateDirectoryW(win::fromUtf8(logDir).c_str(), nullptr);
    Logger::instance().start(logDir + "\\cli.jsonl");
    int rc = 0;
    try {
        if (a.command == "devices") rc = cmdDevices();
        else if (a.command == "measure") rc = cmdMeasure(a);
        else if (a.command == "calibrate") rc = cmdCalibrate(a);
        else if (a.command == "record") rc = cmdRecord(a);
        else if (a.command == "analyze") rc = cmdAnalyze(a);
        else if (a.command == "compare") rc = cmdCompare(a);
        else if (a.command == "bench") rc = cmdBench(a);
        else if (a.command == "run") rc = cmdRun(a);
        else {
            usage();
            rc = a.command.empty() || a.command == "help" ? 0 : 1;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        rc = 1;
    }
    Logger::instance().stop();
    return rc;
}
