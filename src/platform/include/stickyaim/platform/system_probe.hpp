#pragma once

#include <string>
#include <vector>

namespace sa::win {

struct ServiceStatus {
    std::string name;
    bool installed = false;
    bool running = false;
    std::string state;  // "running", "stopped", "not installed", ...
};

ServiceStatus queryService(const std::string& serviceName);

struct LauncherStatus {
    std::string id;           // "steam", "battlenet", "xbox"
    std::string name;
    bool installed = false;
    bool running = false;
    std::string installPath;  // when discoverable
    std::string detection;    // how `installed` was determined
    std::vector<std::string> notes;  // known interaction points; never test results
};

std::vector<LauncherStatus> probeLaunchers();

bool isProcessRunning(const std::string& exeName);
std::vector<std::string> runningProcesses(const std::vector<std::string>& exeNames);

struct DriverReport {
    ServiceStatus vigemBus;
    ServiceStatus hidHide;
    ServiceStatus xusb;  // Microsoft Xbox 360 controller driver
    bool isElevated = false;
};

DriverReport probeDrivers();
bool processIsElevated();

}  // namespace sa::win
