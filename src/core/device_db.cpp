#include "stickyaim/core/device_db.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "stickyaim/core/config_io.hpp"
#include "stickyaim/core/types.hpp"

namespace sa {
namespace {

struct UsageName {
    const char* name;
    std::uint16_t usage;
};
constexpr UsageName kUsages[] = {{"X", 0x30}, {"Y", 0x31}, {"Z", 0x32}, {"Rx", 0x33}, {"Ry", 0x34}, {"Rz", 0x35},
                                 {"Slider", 0x36}, {"Dial", 0x37}};

struct ButtonName {
    const char* name;
    std::uint16_t bit;
};
constexpr ButtonName kButtons[] = {{"A", Button::A},           {"B", Button::B},
                                   {"X", Button::X},           {"Y", Button::Y},
                                   {"LB", Button::LeftShoulder}, {"RB", Button::RightShoulder},
                                   {"Back", Button::Back},     {"Start", Button::Start},
                                   {"LS", Button::LeftThumb},  {"RS", Button::RightThumb},
                                   {"Guide", Button::Guide},   {"DpadUp", Button::DpadUp},
                                   {"DpadDown", Button::DpadDown}, {"DpadLeft", Button::DpadLeft},
                                   {"DpadRight", Button::DpadRight}};

std::uint16_t parseHex(const nlohmann::json& j) {
    if (j.is_number_unsigned() || j.is_number_integer()) return static_cast<std::uint16_t>(j.get<int>());
    if (j.is_string()) return static_cast<std::uint16_t>(std::stoul(j.get<std::string>(), nullptr, 16));
    return 0;
}

}  // namespace

std::string hex4(std::uint16_t v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04X", v);
    return buf;
}

bool parseAxisSpec(const std::string& specIn, HidAxisSource& out) {
    out = HidAxisSource{};
    std::string spec = specIn;
    if (spec.empty() || spec == "none") return true;
    if (spec.front() == '-') {
        out.invert = true;
        spec.erase(spec.begin());
    }
    HidAxisSource::Kind kind = HidAxisSource::Kind::Full;
    if (!spec.empty() && (spec.back() == '+' || spec.back() == '-')) {
        kind = spec.back() == '+' ? HidAxisSource::Kind::PositiveHalf : HidAxisSource::Kind::NegativeHalf;
        spec.pop_back();
    }
    for (const auto& u : kUsages) {
        if (spec == u.name) {
            out.kind = kind;
            out.usage = u.usage;
            return true;
        }
    }
    return false;
}

std::string axisSpecToString(const HidAxisSource& a) {
    if (a.kind == HidAxisSource::Kind::None) return "none";
    std::string s = a.invert ? "-" : "";
    const char* name = "?";
    for (const auto& u : kUsages)
        if (u.usage == a.usage) name = u.name;
    s += name;
    if (a.kind == HidAxisSource::Kind::PositiveHalf) s += "+";
    if (a.kind == HidAxisSource::Kind::NegativeHalf) s += "-";
    return s;
}

HidMapping defaultXInputHidMapping() {
    HidMapping m;
    parseAxisSpec("X", m.lx);
    parseAxisSpec("-Y", m.ly);
    parseAxisSpec("Rx", m.rx);
    parseAxisSpec("-Ry", m.ry);
    // xusb HID shim: Z rests at center; LT moves it one way, RT the other.
    parseAxisSpec("Z+", m.lt);
    parseAxisSpec("Z-", m.rt);
    const std::uint16_t order[] = {Button::A, Button::B, Button::X, Button::Y, Button::LeftShoulder,
                                   Button::RightShoulder, Button::Back, Button::Start, Button::LeftThumb,
                                   Button::RightThumb};
    for (size_t i = 0; i < std::size(order); ++i) m.buttons[i] = order[i];
    return m;
}

HidMapping genericGamepadMapping() {
    HidMapping m;
    parseAxisSpec("X", m.lx);
    parseAxisSpec("-Y", m.ly);
    parseAxisSpec("Z", m.rx);
    parseAxisSpec("-Rz", m.ry);
    parseAxisSpec("Rx", m.lt);
    parseAxisSpec("Ry", m.rt);
    const std::uint16_t order[] = {Button::X, Button::A, Button::B, Button::Y, Button::LeftShoulder,
                                   Button::RightShoulder, 0, 0, Button::Back, Button::Start,
                                   Button::LeftThumb, Button::RightThumb, Button::Guide};
    for (size_t i = 0; i < std::size(order); ++i) m.buttons[i] = order[i];
    return m;
}

KnownDevice knownDeviceFromJson(const nlohmann::json& j) {
    KnownDevice d;
    d.name = j.value("name", "");
    d.vendorId = parseHex(j.value("vendor_id", nlohmann::json("0")));
    if (auto it = j.find("modes"); it != j.end() && it->is_array())
        for (const auto& m : *it) {
            DeviceMode mode;
            mode.productId = parseHex(m.value("product_id", nlohmann::json("0")));
            mode.mode = m.value("mode", "");
            mode.connection = m.value("connection", "");
            mode.verification = m.value("verification", "");
            d.modes.push_back(mode);
        }
    if (auto it = j.find("notes"); it != j.end() && it->is_array())
        for (const auto& n : *it) d.notes.push_back(n.get<std::string>());
    if (auto it = j.find("unverified"); it != j.end() && it->is_array())
        for (const auto& n : *it) d.unverified.push_back(n.get<std::string>());
    if (auto it = j.find("hid_mapping"); it != j.end() && it->is_object()) {
        d.hasHidMapping = true;
        const std::string preset = it->value("preset", "");
        d.hidMapping = preset == "xinput_hid" ? defaultXInputHidMapping() : genericGamepadMapping();
        for (auto [key, axis] : {std::pair{"lx", &d.hidMapping.lx}, std::pair{"ly", &d.hidMapping.ly},
                                 std::pair{"rx", &d.hidMapping.rx}, std::pair{"ry", &d.hidMapping.ry},
                                 std::pair{"lt", &d.hidMapping.lt}, std::pair{"rt", &d.hidMapping.rt}})
            if (it->contains(key)) parseAxisSpec((*it)[key].get<std::string>(), *axis);
        if (auto b = it->find("buttons"); b != it->end() && b->is_object()) {
            d.hidMapping.buttons.fill(0);
            for (const auto& [num, name] : b->items()) {
                const int idx = std::stoi(num) - 1;
                if (idx < 0 || idx >= 32) continue;
                for (const auto& bn : kButtons)
                    if (name.get<std::string>() == bn.name) d.hidMapping.buttons[static_cast<size_t>(idx)] = bn.bit;
            }
        }
        d.hidMapping.hatToDpad = it->value("hat_to_dpad", true);
        d.hidMapping.verified = it->value("verified", false);
    }
    return d;
}

std::vector<KnownDevice> loadDeviceDatabase(const std::string& dir, std::vector<std::string>* errors) {
    namespace fs = std::filesystem;
    std::vector<KnownDevice> out;
    std::error_code ec;
    const fs::path root(std::u8string(dir.begin(), dir.end()));
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (e.path().extension() != ".json") continue;
        std::string text, err;
        if (!readTextFile(e.path().string(), text, &err)) {
            if (errors) errors->push_back(err);
            continue;
        }
        const auto j = nlohmann::json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            if (errors) errors->push_back("invalid JSON: " + e.path().string());
            continue;
        }
        try {
            KnownDevice d = knownDeviceFromJson(j);
            d.id = e.path().stem().string();
            out.push_back(std::move(d));
        } catch (const std::exception& ex) {
            if (errors) errors->push_back(e.path().string() + ": " + ex.what());
        }
    }
    return out;
}

const KnownDevice* matchDevice(const std::vector<KnownDevice>& db, std::uint16_t vid, std::uint16_t pid,
                               const DeviceMode** mode) {
    for (const auto& d : db) {
        if (d.vendorId != vid) continue;
        for (const auto& m : d.modes)
            if (m.productId == pid) {
                if (mode) *mode = &m;
                return &d;
            }
    }
    return nullptr;
}

}  // namespace sa
