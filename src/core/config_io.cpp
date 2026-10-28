#include "stickyaim/core/config_io.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace sa {
namespace {

using nlohmann::json;

std::filesystem::path toPath(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

template <typename T>
void get(const json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it != j.end() && !it->is_null()) out = it->get<T>();
}

json curveToJson(const CurveSpec& c) {
    json pts = json::array();
    for (const auto& p : c.points) pts.push_back({p.x, p.y});
    return {{"type", toString(c.type)}, {"exponent", c.exponent}, {"k", c.expK}, {"points", pts}};
}

CurveSpec curveFromJson(const json& j) {
    CurveSpec c;
    if (!j.is_object()) return c;
    std::string type;
    get(j, "type", type);
    curveTypeFromString(type, c.type);
    get(j, "exponent", c.exponent);
    get(j, "k", c.expK);
    if (auto it = j.find("points"); it != j.end() && it->is_array())
        for (const auto& p : *it)
            if (p.is_array() && p.size() == 2) c.points.push_back({p[0].get<float>(), p[1].get<float>()});
    return c;
}

json zoneToJson(const SlopeZone& z) {
    return {{"enabled", z.enabled}, {"start", z.start}, {"end", z.end}, {"gain", z.gain}, {"blend", z.blend}};
}

SlopeZone zoneFromJson(const json& j, SlopeZone z) {
    if (!j.is_object()) return z;
    get(j, "enabled", z.enabled);
    get(j, "start", z.start);
    get(j, "end", z.end);
    get(j, "gain", z.gain);
    get(j, "blend", z.blend);
    return z;
}

json shapeToJson(const ResponseShape& s) {
    return {{"curve", curveToJson(s.curve)}, {"fine", zoneToJson(s.fine)}, {"fast", zoneToJson(s.fast)}};
}

ResponseShape shapeFromJson(const json& j) {
    ResponseShape s;
    if (!j.is_object()) return s;
    if (j.contains("curve")) s.curve = curveFromJson(j["curve"]);
    if (j.contains("fine")) s.fine = zoneFromJson(j["fine"], s.fine);
    if (j.contains("fast")) s.fast = zoneFromJson(j["fast"], s.fast);
    return s;
}

json deadzoneToJson(const DeadzoneConfig& d) {
    return {{"type", toString(d.type)}, {"inner", d.inner}, {"outer", d.outer}};
}

DeadzoneConfig deadzoneFromJson(const json& j, DeadzoneConfig d) {
    if (!j.is_object()) return d;
    std::string type;
    get(j, "type", type);
    deadzoneTypeFromString(type, d.type);
    get(j, "inner", d.inner);
    get(j, "outer", d.outer);
    return d;
}

json smoothingToJson(const SmoothingConfig& s) {
    return {{"type", toString(s.type)},       {"ema_tau_ms", s.emaTauMs}, {"min_cutoff_hz", s.minCutoffHz},
            {"beta", s.beta},                 {"d_cutoff_hz", s.dCutoffHz},
            {"flick_bypass_speed", s.flickBypassSpeed}};
}

SmoothingConfig smoothingFromJson(const json& j) {
    SmoothingConfig s;
    if (!j.is_object()) return s;
    std::string type;
    get(j, "type", type);
    smoothingTypeFromString(type, s.type);
    get(j, "ema_tau_ms", s.emaTauMs);
    get(j, "min_cutoff_hz", s.minCutoffHz);
    get(j, "beta", s.beta);
    get(j, "d_cutoff_hz", s.dCutoffHz);
    get(j, "flick_bypass_speed", s.flickBypassSpeed);
    return s;
}

json contextToJson(const AimContextConfig& c) {
    return {{"deadzone", deadzoneToJson(c.deadzone)},
            {"curve_mode", toString(c.curveMode)},
            {"radial", shapeToJson(c.radial)},
            {"x", shapeToJson(c.x)},
            {"y", shapeToJson(c.y)},
            {"sens_x", c.sensX},
            {"sens_y", c.sensY},
            {"smoothing", smoothingToJson(c.smoothing)},
            {"circular_saturation", c.circularSaturation},
            {"anti_deadzone", c.antiDeadzone}};
}

AimContextConfig contextFromJson(const json& j) {
    AimContextConfig c;
    if (!j.is_object()) return c;
    if (j.contains("deadzone")) c.deadzone = deadzoneFromJson(j["deadzone"], c.deadzone);
    std::string mode;
    get(j, "curve_mode", mode);
    curveModeFromString(mode, c.curveMode);
    if (j.contains("radial")) c.radial = shapeFromJson(j["radial"]);
    if (j.contains("x")) c.x = shapeFromJson(j["x"]);
    if (j.contains("y")) c.y = shapeFromJson(j["y"]);
    get(j, "sens_x", c.sensX);
    get(j, "sens_y", c.sensY);
    if (j.contains("smoothing")) c.smoothing = smoothingFromJson(j["smoothing"]);
    get(j, "circular_saturation", c.circularSaturation);
    get(j, "anti_deadzone", c.antiDeadzone);
    return c;
}

json stickCalToJson(const StickCalibration& s) {
    return {{"center_x", s.centerX}, {"center_y", s.centerY}, {"max_pos_x", s.maxPosX},
            {"max_neg_x", s.maxNegX}, {"max_pos_y", s.maxPosY}, {"max_neg_y", s.maxNegY}};
}

StickCalibration stickCalFromJson(const json& j) {
    StickCalibration s;
    if (!j.is_object()) return s;
    get(j, "center_x", s.centerX);
    get(j, "center_y", s.centerY);
    get(j, "max_pos_x", s.maxPosX);
    get(j, "max_neg_x", s.maxNegX);
    get(j, "max_pos_y", s.maxPosY);
    get(j, "max_neg_y", s.maxNegY);
    return s;
}

}  // namespace

json toJson(const Profile& p) {
    return {{"schema_version", p.schemaVersion},
            {"name", p.name},
            {"description", p.description},
            {"target_device", p.targetDevice},
            {"hip", contextToJson(p.hip)},
            {"ads", contextToJson(p.ads)},
            {"ads_from_left_trigger", p.adsFromLeftTrigger},
            {"ads_enter", p.adsEnter},
            {"ads_exit", p.adsExit},
            {"context_transition_ms", p.contextTransitionMs},
            {"move_stick", deadzoneToJson(p.moveStick)},
            {"left_trigger", {{"inner", p.leftTrigger.inner}, {"outer", p.leftTrigger.outer}}},
            {"right_trigger", {{"inner", p.rightTrigger.inner}, {"outer", p.rightTrigger.outer}}}};
}

Profile profileFromJson(const json& j) {
    Profile p;
    if (!j.is_object()) return p;
    get(j, "schema_version", p.schemaVersion);
    get(j, "name", p.name);
    get(j, "description", p.description);
    get(j, "target_device", p.targetDevice);
    if (j.contains("hip")) p.hip = contextFromJson(j["hip"]);
    if (j.contains("ads")) p.ads = contextFromJson(j["ads"]);
    get(j, "ads_from_left_trigger", p.adsFromLeftTrigger);
    get(j, "ads_enter", p.adsEnter);
    get(j, "ads_exit", p.adsExit);
    get(j, "context_transition_ms", p.contextTransitionMs);
    if (j.contains("move_stick")) p.moveStick = deadzoneFromJson(j["move_stick"], p.moveStick);
    for (auto [key, trig] : {std::pair{"left_trigger", &p.leftTrigger}, std::pair{"right_trigger", &p.rightTrigger}}) {
        if (auto it = j.find(key); it != j.end() && it->is_object()) {
            get(*it, "inner", trig->inner);
            get(*it, "outer", trig->outer);
        }
    }
    return p;
}

json toJson(const DeviceCalibration& c) {
    return {{"device_key", c.deviceKey}, {"left", stickCalToJson(c.left)}, {"right", stickCalToJson(c.right)}};
}

DeviceCalibration calibrationFromJson(const json& j) {
    DeviceCalibration c;
    if (!j.is_object()) return c;
    get(j, "device_key", c.deviceKey);
    if (j.contains("left")) c.left = stickCalFromJson(j["left"]);
    if (j.contains("right")) c.right = stickCalFromJson(j["right"]);
    return c;
}

bool writeTextFile(const std::string& path, const std::string& text, std::string* error) {
    namespace fs = std::filesystem;
    const fs::path target = toPath(path);
    std::error_code ec;
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    // Write to a temp file and rename, so a crash never leaves a half-written profile.
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (error) *error = "cannot open " + path + ".tmp for writing";
            return false;
        }
        f << text;
        if (!f) {
            if (error) *error = "write failed for " + path;
            return false;
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        if (error) *error = "rename failed: " + ec.message();
        return false;
    }
    return true;
}

bool readTextFile(const std::string& path, std::string& out, std::string* error) {
    std::ifstream f(toPath(path), std::ios::binary);
    if (!f) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool saveProfile(const Profile& p, const std::string& path, std::string* error) {
    return writeTextFile(path, toJson(p).dump(2) + "\n", error);
}

bool loadProfile(const std::string& path, Profile& out, std::string* error) {
    std::string text;
    if (!readTextFile(path, text, error)) return false;
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (error) *error = "invalid JSON in " + path;
        return false;
    }
    Profile p;
    try {
        p = profileFromJson(j);
    } catch (const std::exception& e) {
        if (error) *error = std::string("bad field type: ") + e.what();
        return false;
    }
    const auto errs = validate(p);
    if (!errs.empty()) {
        if (error) *error = "profile does not validate: " + errs.front();
        return false;
    }
    out = std::move(p);
    return true;
}

bool saveCalibration(const DeviceCalibration& c, const std::string& path, std::string* error) {
    return writeTextFile(path, toJson(c).dump(2) + "\n", error);
}

bool loadCalibration(const std::string& path, DeviceCalibration& out, std::string* error) {
    std::string text;
    if (!readTextFile(path, text, error)) return false;
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (error) *error = "invalid JSON in " + path;
        return false;
    }
    try {
        out = calibrationFromJson(j);
    } catch (const std::exception& e) {
        if (error) *error = std::string("bad field type: ") + e.what();
        return false;
    }
    return true;
}

}  // namespace sa
