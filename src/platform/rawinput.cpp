#include "stickyaim/platform/rawinput.hpp"

#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <algorithm>
#include <cwctype>

#include "stickyaim/core/clock.hpp"
#include "stickyaim/core/logger.hpp"
#include "stickyaim/platform/win_util.hpp"

namespace sa::win {
namespace {

constexpr wchar_t kWindowClass[] = L"StickyAimLabRawInput";
constexpr UINT kMsgQuit = WM_APP + 1;

std::string deviceName(HANDLE h) {
    UINT size = 0;
    GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, nullptr, &size);
    if (size == 0) return {};
    std::wstring name(size, L'\0');
    if (GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, name.data(), &size) == static_cast<UINT>(-1)) return {};
    name.resize(wcslen(name.c_str()));
    return toUtf8(name);
}

bool deviceInfo(HANDLE h, RID_DEVICE_INFO& info) {
    info = {};
    info.cbSize = sizeof(info);
    UINT size = sizeof(info);
    return GetRawInputDeviceInfoW(h, RIDI_DEVICEINFO, &info, &size) != static_cast<UINT>(-1);
}

int parseInterface(const std::string& path) {
    std::string upper = path;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const auto p = upper.find("MI_");
    if (p == std::string::npos || p + 5 > upper.size()) return -1;
    return std::stoi(upper.substr(p + 3, 2), nullptr, 16);
}

void fillStrings(HidDeviceInfo& d) {
    const std::wstring wpath = fromUtf8(d.path);
    HANDLE f = CreateFileW(wpath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    wchar_t buf[256];
    if (HidD_GetProductString(f, buf, sizeof(buf))) d.product = toUtf8(buf);
    if (HidD_GetManufacturerString(f, buf, sizeof(buf))) d.manufacturer = toUtf8(buf);
    CloseHandle(f);
}

HidDeviceInfo describe(HANDLE h) {
    HidDeviceInfo d;
    d.path = deviceName(h);
    RID_DEVICE_INFO info;
    if (deviceInfo(h, info) && info.dwType == RIM_TYPEHID) {
        d.vendorId = static_cast<std::uint16_t>(info.hid.dwVendorId);
        d.productId = static_cast<std::uint16_t>(info.hid.dwProductId);
        d.version = static_cast<std::uint16_t>(info.hid.dwVersionNumber);
        d.usagePage = info.hid.usUsagePage;
        d.usage = info.hid.usUsage;
    }
    d.interfaceNumber = parseInterface(d.path);
    std::string upper = d.path;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    d.xinputHidInterface = upper.find("IG_") != std::string::npos;
    fillStrings(d);
    return d;
}

bool isGameController(const HidDeviceInfo& d) { return d.usagePage == 0x01 && (d.usage == 0x04 || d.usage == 0x05 || d.usage == 0x08); }

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<RawInputMonitor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            break;
        }
        case WM_INPUT:
            if (self) self->onInput(reinterpret_cast<void*>(lp));
            break;  // DefWindowProc performs required cleanup for RIM_INPUT
        case WM_INPUT_DEVICE_CHANGE:
            if (self) self->onDeviceChange(reinterpret_cast<void*>(lp), wp == GIDC_ARRIVAL);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

float normalizeValue(LONG v, const HIDP_VALUE_CAPS& c) {
    LONG mn = c.LogicalMin, mx = c.LogicalMax;
    ULONG uv = static_cast<ULONG>(v);
    if (mn >= mx) {  // descriptor declares an unsigned range that overflowed a signed LONG
        mn = 0;
        mx = c.BitSize >= 32 ? 0x7fffffff : static_cast<LONG>((1ul << c.BitSize) - 1);
    }
    if (c.BitSize < 32) uv &= (1ul << c.BitSize) - 1;
    double val = static_cast<double>(mn < 0 ? v : static_cast<LONG>(uv));
    const double t = (val - mn) / static_cast<double>(mx - mn);
    return static_cast<float>(std::clamp(t, 0.0, 1.0) * 2.0 - 1.0);  // [-1, 1]
}

}  // namespace

std::string HidDeviceInfo::usageName() const {
    if (usagePage == 0x01) {
        switch (usage) {
            case 0x02: return "mouse";
            case 0x04: return "joystick";
            case 0x05: return "gamepad";
            case 0x06: return "keyboard";
            case 0x08: return "multi-axis controller";
            case 0x80: return "system control";
            default: break;
        }
    }
    if (usagePage == 0x0C) return "consumer control";
    if (usagePage >= 0xFF00) return "vendor-defined (0x" + hex4(usagePage) + ")";
    return "page 0x" + hex4(usagePage) + " usage 0x" + hex4(usage);
}

std::vector<HidDeviceInfo> enumerateHidDevices(bool gameRelatedOnly) {
    UINT count = 0;
    GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST));
    std::vector<RAWINPUTDEVICELIST> list(count);
    if (count == 0 || GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) return {};
    std::vector<HidDeviceInfo> all;
    for (UINT i = 0; i < count; ++i)
        if (list[i].dwType == RIM_TYPEHID) all.push_back(describe(list[i].hDevice));
    if (!gameRelatedOnly) return all;
    std::vector<std::uint16_t> controllerVids;
    for (const auto& d : all)
        if (isGameController(d)) controllerVids.push_back(d.vendorId);
    std::vector<HidDeviceInfo> out;
    for (const auto& d : all)
        if (isGameController(d) || std::find(controllerVids.begin(), controllerVids.end(), d.vendorId) != controllerVids.end())
            out.push_back(d);
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return std::tie(a.vendorId, a.productId, a.interfaceNumber, a.path) < std::tie(b.vendorId, b.productId, b.interfaceNumber, b.path);
    });
    return out;
}

// ------------------------------------------------------------------ monitor

struct RawInputMonitor::DeviceState {
    HidDeviceInfo info;
    std::vector<std::uint8_t> preparsed;
    std::vector<HIDP_VALUE_CAPS> valueCaps;
    bool decodable = false;
    bool present = true;
    std::uint64_t reports = 0, messages = 0, coalesced = 0;
    TimingAnalyzer timing{3.0, 1.0, 50000};
    GamepadState lastDecoded;
    std::vector<std::uint8_t> lastReport;
    std::vector<USAGE> usageBuf;
};

RawInputMonitor::RawInputMonitor() : stream_(4096) {}

RawInputMonitor::~RawInputMonitor() { stop(); }

bool RawInputMonitor::start(std::string* error) {
    if (running_.load()) return true;
    startError_.clear();
    std::atomic<int> ready{0};
    thread_ = std::thread([this, &ready] {
        threadId_.store(GetCurrentThreadId());
        HINSTANCE inst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = wndProc;
        wc.hInstance = inst;
        wc.lpszClassName = kWindowClass;
        RegisterClassExW(&wc);  // fails harmlessly if already registered
        HWND hwnd = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, inst, this);
        if (!hwnd) {
            startError_ = "CreateWindowEx failed: " + lastErrorMessage(GetLastError());
            ready.store(-1);
            return;
        }
        RAWINPUTDEVICE rid[3] = {
            {0x01, 0x04, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, hwnd},
            {0x01, 0x05, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, hwnd},
            {0x01, 0x08, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, hwnd},
        };
        if (!RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE))) {
            startError_ = "RegisterRawInputDevices failed: " + lastErrorMessage(GetLastError());
            DestroyWindow(hwnd);
            ready.store(-1);
            return;
        }
        running_.store(true);
        ready.store(1);
        run();
        for (auto& r : rid) {
            r.dwFlags = RIDEV_REMOVE;
            r.hwndTarget = nullptr;
        }
        RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE));
        DestroyWindow(hwnd);
        running_.store(false);
    });
    while (ready.load() == 0) std::this_thread::yield();
    if (ready.load() < 0) {
        thread_.join();
        if (error) *error = startError_;
        return false;
    }
    return true;
}

void RawInputMonitor::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == kMsgQuit) break;
        DispatchMessageW(&msg);
    }
}

void RawInputMonitor::stop() {
    if (thread_.joinable()) {
        PostThreadMessageW(threadId_.load(), kMsgQuit, 0, 0);
        thread_.join();
    }
}

RawInputMonitor::DeviceState* RawInputMonitor::deviceFor(void* handle) {
    auto it = devices_.find(handle);
    if (it != devices_.end()) return it->second.get();
    auto st = std::make_unique<DeviceState>();
    st->info = describe(static_cast<HANDLE>(handle));
    UINT size = 0;
    GetRawInputDeviceInfoW(static_cast<HANDLE>(handle), RIDI_PREPARSEDDATA, nullptr, &size);
    if (size > 0) {
        st->preparsed.resize(size);
        if (GetRawInputDeviceInfoW(static_cast<HANDLE>(handle), RIDI_PREPARSEDDATA, st->preparsed.data(), &size) != static_cast<UINT>(-1)) {
            auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(st->preparsed.data());
            HIDP_CAPS caps;
            if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS) {
                USHORT n = caps.NumberInputValueCaps;
                st->valueCaps.resize(n);
                if (n == 0 || HidP_GetValueCaps(HidP_Input, st->valueCaps.data(), &n, pp) == HIDP_STATUS_SUCCESS) {
                    st->valueCaps.resize(n);
                    st->decodable = true;
                }
                st->usageBuf.resize(HidP_MaxUsageListLength(HidP_Input, HID_USAGE_PAGE_BUTTON, pp) + 1);
            }
        }
    }
    auto* raw = st.get();
    devices_.emplace(handle, std::move(st));
    logInfo("rawinput", "HID device seen: " + raw->info.path);
    return raw;
}

namespace {

// Decodes one report with `mapping`. Returns false if the device has no
// usable value caps.
bool decode(const std::vector<std::uint8_t>& preparsed, const std::vector<HIDP_VALUE_CAPS>& caps,
            std::vector<USAGE>& usageBuf, const HidMapping& m, const std::uint8_t* report, ULONG len, GamepadState& out) {
    auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<std::uint8_t*>(preparsed.data()));
    auto* rep = reinterpret_cast<PCHAR>(const_cast<std::uint8_t*>(report));
    auto readAxis = [&](const HidAxisSource& a) -> float {
        if (a.kind == HidAxisSource::Kind::None) return 0.0f;
        for (const auto& c : caps) {
            const bool match = c.UsagePage == a.usagePage &&
                               (c.IsRange ? (a.usage >= c.Range.UsageMin && a.usage <= c.Range.UsageMax) : c.NotRange.Usage == a.usage);
            if (!match) continue;
            ULONG v = 0;
            if (HidP_GetUsageValue(HidP_Input, a.usagePage, c.LinkCollection, a.usage, &v, pp, rep, len) != HIDP_STATUS_SUCCESS)
                return 0.0f;
            float n = normalizeValue(static_cast<LONG>(v), c);
            if (a.invert) n = -n;
            switch (a.kind) {
                case HidAxisSource::Kind::PositiveHalf: return std::max(0.0f, n);
                case HidAxisSource::Kind::NegativeHalf: return std::max(0.0f, -n);
                default: return n;
            }
        }
        return 0.0f;
    };
    out = GamepadState{};
    out.left = {readAxis(m.lx), readAxis(m.ly)};
    out.right = {readAxis(m.rx), readAxis(m.ry)};
    // Triggers given as full-range axes map [-1, 1] -> [0, 1].
    auto trig = [&](const HidAxisSource& a) {
        const float v = readAxis(a);
        return a.kind == HidAxisSource::Kind::Full ? (v + 1.0f) * 0.5f : v;
    };
    out.lt = trig(m.lt);
    out.rt = trig(m.rt);

    ULONG n = static_cast<ULONG>(usageBuf.size());
    if (n > 0 && HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usageBuf.data(), &n, pp, rep, len) == HIDP_STATUS_SUCCESS) {
        for (ULONG i = 0; i < n; ++i) {
            const int idx = static_cast<int>(usageBuf[i]) - 1;
            if (idx >= 0 && idx < 32) out.buttons |= m.buttons[static_cast<size_t>(idx)];
        }
    }
    if (m.hatToDpad) {
        for (const auto& c : caps) {
            if (c.UsagePage != 0x01 || c.IsRange || c.NotRange.Usage != 0x39) continue;
            ULONG v = 0;
            if (HidP_GetUsageValue(HidP_Input, 0x01, c.LinkCollection, 0x39, &v, pp, rep, len) != HIDP_STATUS_SUCCESS) break;
            const LONG hat = static_cast<LONG>(v) - c.LogicalMin;
            static constexpr std::uint16_t dirs[8] = {
                Button::DpadUp,   Button::DpadUp | Button::DpadRight,  Button::DpadRight, Button::DpadDown | Button::DpadRight,
                Button::DpadDown, Button::DpadDown | Button::DpadLeft, Button::DpadLeft,  Button::DpadUp | Button::DpadLeft};
            if (hat >= 0 && hat < 8 && static_cast<LONG>(v) <= c.LogicalMax) out.buttons |= dirs[hat];
            break;
        }
    }
    return true;
}

}  // namespace

void RawInputMonitor::onInput(void* hRawInput) {
    const TimestampNs t = nowNs();
    UINT size = 0;
    GetRawInputData(static_cast<HRAWINPUT>(hRawInput), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    if (size == 0) return;
    if (buffer_.size() < size) buffer_.resize(size);
    if (GetRawInputData(static_cast<HRAWINPUT>(hRawInput), RID_INPUT, buffer_.data(), &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        return;
    const auto* ri = reinterpret_cast<const RAWINPUT*>(buffer_.data());
    if (ri->header.dwType != RIM_TYPEHID) return;

    std::lock_guard lk(mu_);
    DeviceState* d = deviceFor(ri->header.hDevice);
    d->present = true;
    const DWORD count = ri->data.hid.dwCount;
    const DWORD each = ri->data.hid.dwSizeHid;
    ++d->messages;
    if (count > 1) ++d->coalesced;
    const bool streaming = !streamPath_.empty() && d->info.path == streamPath_;
    for (DWORD i = 0; i < count; ++i) {
        const std::uint8_t* rep = ri->data.hid.bRawData + static_cast<size_t>(i) * each;
        ++d->reports;
        // All reports in one message share the receive time; spread is unknown.
        d->timing.add(t, 0, false);
        if (i + 1 == count) d->lastReport.assign(rep, rep + each);
        if (d->decodable) {
            const HidMapping& mapping = streaming ? streamMapping_ : (d->info.xinputHidInterface ? defaultXInputHidMapping() : genericGamepadMapping());
            GamepadState gs;
            decode(d->preparsed, d->valueCaps, d->usageBuf, mapping, rep, each, gs);
            d->lastDecoded = gs;
            if (streaming) {
                RawSample s;
                s.t = t;
                s.sequence = ++streamSeq_;
                s.pad = denormalize(gs);
                if (!stream_.push(s)) streamDrops_.fetch_add(1);
            }
        }
    }
}

void RawInputMonitor::onDeviceChange(void* hDevice, bool arrived) {
    std::lock_guard lk(mu_);
    if (arrived) {
        deviceFor(hDevice)->present = true;
    } else if (auto it = devices_.find(hDevice); it != devices_.end()) {
        it->second->present = false;
        logInfo("rawinput", "HID device removed: " + it->second->info.path);
    }
}

std::vector<HidReportStats> RawInputMonitor::snapshot() const {
    std::lock_guard lk(mu_);
    std::vector<HidReportStats> out;
    for (const auto& [h, d] : devices_) {
        if (!d->present) continue;
        HidReportStats s;
        s.info = d->info;
        s.reports = d->reports;
        s.messages = d->messages;
        s.coalescedMessages = d->coalesced;
        s.timing = d->timing.summary();
        s.decodable = d->decodable;
        s.lastDecoded = d->lastDecoded;
        s.lastReport = d->lastReport;
        out.push_back(std::move(s));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.info.path < b.info.path; });
    return out;
}

void RawInputMonitor::resetStats() {
    std::lock_guard lk(mu_);
    for (auto& [h, d] : devices_) {
        d->reports = d->messages = d->coalesced = 0;
        d->timing.reset();
    }
}

void RawInputMonitor::setStreamDevice(const std::string& path, const HidMapping& mapping) {
    std::lock_guard lk(mu_);
    streamPath_ = path;
    streamMapping_ = mapping;
}

void RawInputMonitor::clearStreamDevice() {
    std::lock_guard lk(mu_);
    streamPath_.clear();
}

bool RawInputMonitor::isPresent(const std::string& path) const {
    std::lock_guard lk(mu_);
    for (const auto& [h, d] : devices_)
        if (d->present && d->info.path == path) return true;
    return false;
}

// ------------------------------------------------------------------ HidSource

HidSource::HidSource(std::shared_ptr<RawInputMonitor> monitor, std::string path, HidMapping mapping, std::string label)
    : monitor_(std::move(monitor)), path_(std::move(path)), mapping_(mapping), label_(std::move(label)) {}

bool HidSource::open() {
    if (!monitor_->running() && !monitor_->start()) return false;
    // Check the device is still enumerated.
    bool found = false;
    for (const auto& d : enumerateHidDevices(true))
        if (d.path == path_) found = true;
    if (!found) return false;
    monitor_->setStreamDevice(path_, mapping_);
    present_.store(true);
    return true;
}

void HidSource::close() { monitor_->clearStreamDevice(); }

PollResult HidSource::poll(RawSample& out) noexcept {
    RawSample s;
    bool got = false;
    while (monitor_->stream().pop(s)) {
        if (got) ++coalesced_;
        out = s;
        got = true;
    }
    if (got) return PollResult::NewSample;
    // Presence is checked at most every 100 ms (it takes the monitor's mutex).
    const TimestampNs now = nowNs();
    if (now - lastPresenceCheck_ > 100'000'000) {
        lastPresenceCheck_ = now;
        present_.store(monitor_->isPresent(path_));
    }
    return present_.load() ? PollResult::NoChange : PollResult::Disconnected;
}

}  // namespace sa::win
