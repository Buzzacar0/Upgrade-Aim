#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One-step HidHide setup: games see only the virtual controller while
// Upgrade Aim (allow-listed) still reads the physical one. Changing HidHide
// needs administrator rights, so the app re-runs itself elevated with
// --hidhide-setup and this code does the work in that process.
namespace sa::win::hidhide {

std::string cliPath();  // HidHideCLI.exe, empty if HidHide is not installed
bool installed();

// Present device instances of a physical controller that HidHide should hide:
// its HID game/vendor collections and its XInput (XnaComposite) interface.
std::vector<std::string> controllerInstances(std::uint16_t vid, std::uint16_t pid);

// Runs in the elevated process. Returns 0 on success.
//  setup: allow-list `appExe`, drop stale entries of this VID/PID and any entry
//         for the virtual pad (VID_045E PID_028E), hide the present instances,
//         turn hiding on.
//  undo:  unhide every entry of this VID/PID.
int run(std::uint16_t vid, std::uint16_t pid, bool undo, const std::string& appExe, std::string* log);

}  // namespace sa::win::hidhide
