#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace sa {

// How raw HID usages map onto the logical XInput-style gamepad. Used by the
// Raw Input source for controllers that are not in XInput mode.
struct HidAxisSource {
    enum class Kind { None, Full, PositiveHalf, NegativeHalf } kind = Kind::None;
    std::uint16_t usagePage = 0x01;
    std::uint16_t usage = 0;
    bool invert = false;
};

struct HidMapping {
    HidAxisSource lx, ly, rx, ry, lt, rt;
    // HID button number (1-based, usage page 0x09) -> XInput button bit.
    std::array<std::uint16_t, 32> buttons{};
    bool hatToDpad = true;
    bool verified = false;  // true only when confirmed on real hardware
};

// Parses an axis spec like "X", "-Y", "Rx", "Z+", "Z-", "Rz", "none".
bool parseAxisSpec(const std::string& spec, HidAxisSource& out);
std::string axisSpecToString(const HidAxisSource& a);

// Mapping used by the Windows "XInput HID compatibility" collection (IG_xx
// interfaces): X/Y left stick, Rx/Ry right stick (Y up = smaller value), Z
// combined triggers. Marked unverified until confirmed per device.
HidMapping defaultXInputHidMapping();
HidMapping genericGamepadMapping();

struct DeviceMode {
    std::uint16_t productId = 0;
    std::string mode;          // e.g. "XInput (wired USB)"
    std::string connection;    // e.g. "USB cable", "2.4 GHz dongle"
    std::string verification;  // what was actually observed, by whom/when; empty = unverified
};

struct KnownDevice {
    std::string id;            // file stem
    std::string name;
    std::uint16_t vendorId = 0;
    std::vector<DeviceMode> modes;
    std::vector<std::string> notes;
    std::vector<std::string> unverified;  // claims we could not measure
    bool hasHidMapping = false;
    HidMapping hidMapping;
};

KnownDevice knownDeviceFromJson(const nlohmann::json& j);
// Loads every *.json in `dir`. Files that fail to parse are reported in `errors`.
std::vector<KnownDevice> loadDeviceDatabase(const std::string& dir, std::vector<std::string>* errors = nullptr);
// Finds a device by VID/PID. `mode` (optional) receives the matching mode.
const KnownDevice* matchDevice(const std::vector<KnownDevice>& db, std::uint16_t vid, std::uint16_t pid,
                               const DeviceMode** mode = nullptr);

std::string hex4(std::uint16_t v);

}  // namespace sa
