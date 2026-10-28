#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "stickyaim/core/device_db.hpp"
#include "stickyaim/core/io.hpp"
#include "stickyaim/core/spsc_queue.hpp"
#include "stickyaim/core/stats.hpp"

namespace sa::win {

struct HidDeviceInfo {
    std::string path;
    std::uint16_t vendorId = 0, productId = 0, version = 0;
    std::uint16_t usagePage = 0, usage = 0;
    std::string product, manufacturer;
    int interfaceNumber = -1;     // MI_xx from the device path, -1 if single-interface
    bool xinputHidInterface = false;  // "IG_" in the path: Windows' HID view of an XInput device
    std::string usageName() const;
};

// Lists HID devices known to Raw Input. With gameRelatedOnly, returns
// joysticks/gamepads plus every other collection of the same VID (so vendor
// configuration interfaces of a controller are visible too).
std::vector<HidDeviceInfo> enumerateHidDevices(bool gameRelatedOnly = true);

struct HidReportStats {
    HidDeviceInfo info;
    std::uint64_t reports = 0;
    std::uint64_t messages = 0;          // WM_INPUT messages
    std::uint64_t coalescedMessages = 0; // messages carrying more than one report
    TimingSummary timing;                // report arrival intervals (host receive time)
    bool decodable = false;
    GamepadState lastDecoded;            // using the device's mapping (may be unverified)
    std::vector<std::uint8_t> lastReport;
};

// Receives HID reports from game controllers through Raw Input
// (RIDEV_INPUTSINK, message-only window on its own thread) and measures their
// arrival timing per device. This is the most direct report-rate measurement
// available without a kernel driver, but it measures arrival at this process,
// which includes driver and message-queue scheduling.
class RawInputMonitor {
public:
    RawInputMonitor();
    ~RawInputMonitor();

    bool start(std::string* error = nullptr);
    void stop();
    bool running() const noexcept { return running_.load(); }

    std::vector<HidReportStats> snapshot() const;
    void resetStats();

    // Streams decoded samples of one device (by device path) to stream().
    void setStreamDevice(const std::string& path, const HidMapping& mapping);
    void clearStreamDevice();
    bool isPresent(const std::string& path) const;
    SpscQueue<RawSample>& stream() noexcept { return stream_; }
    std::uint64_t streamDrops() const noexcept { return streamDrops_.load(); }

    // Called from the window procedure.
    void onInput(void* hRawInput);
    void onDeviceChange(void* hDevice, bool arrived);

private:
    struct DeviceState;
    void run();
    DeviceState* deviceFor(void* handle);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<unsigned long> threadId_{0};
    mutable std::mutex mu_;
    std::unordered_map<void*, std::unique_ptr<DeviceState>> devices_;
    std::vector<std::uint8_t> buffer_;
    std::string streamPath_;
    HidMapping streamMapping_;
    SpscQueue<RawSample> stream_;
    std::atomic<std::uint64_t> streamDrops_{0};
    std::uint32_t streamSeq_ = 0;
    std::string startError_;
};

// IInputSource on top of RawInputMonitor for HID (non-XInput) controllers.
// Returns the newest report on each poll; older queued reports are counted
// as coalesced (they are visible in the Latency Lab).
class HidSource final : public IInputSource {
public:
    HidSource(std::shared_ptr<RawInputMonitor> monitor, std::string path, HidMapping mapping, std::string label);
    std::string name() const override { return label_; }
    bool open() override;
    void close() override;
    PollResult poll(RawSample& out) noexcept override;
    bool hasDeviceSequence() const noexcept override { return true; }
    std::uint64_t coalesced() const noexcept { return coalesced_; }

private:
    std::shared_ptr<RawInputMonitor> monitor_;
    std::string path_;
    HidMapping mapping_;
    std::string label_;
    std::uint64_t coalesced_ = 0;
    std::atomic<bool> present_{false};
    TimestampNs lastPresenceCheck_ = 0;
};

}  // namespace sa::win
