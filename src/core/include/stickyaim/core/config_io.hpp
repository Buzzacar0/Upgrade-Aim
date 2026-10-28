#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "stickyaim/core/calibration.hpp"
#include "stickyaim/core/profile.hpp"

namespace sa {

// JSON (de)serialization. Parsing is tolerant: missing fields keep their
// defaults and unknown fields are ignored, so older files keep loading.
// Out-of-range values are NOT silently fixed — callers run validate().

nlohmann::json toJson(const Profile& p);
Profile profileFromJson(const nlohmann::json& j);

nlohmann::json toJson(const DeviceCalibration& c);
DeviceCalibration calibrationFromJson(const nlohmann::json& j);

// File helpers. Return false and fill `error` on I/O, parse or validation failure.
bool saveProfile(const Profile& p, const std::string& path, std::string* error = nullptr);
bool loadProfile(const std::string& path, Profile& out, std::string* error = nullptr);
bool saveCalibration(const DeviceCalibration& c, const std::string& path, std::string* error = nullptr);
bool loadCalibration(const std::string& path, DeviceCalibration& out, std::string* error = nullptr);

bool writeTextFile(const std::string& path, const std::string& text, std::string* error = nullptr);
bool readTextFile(const std::string& path, std::string& out, std::string* error = nullptr);

}  // namespace sa
