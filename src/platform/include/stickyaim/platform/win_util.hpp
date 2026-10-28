#pragma once

#include <cstdint>
#include <string>

namespace sa::win {

std::string toUtf8(const std::wstring& w);
std::wstring fromUtf8(const std::string& s);
std::string lastErrorMessage(unsigned long code);

// Directory containing the running executable (UTF-8).
std::string executableDir();
// Per-user writable data directory: %APPDATA%\StickyAimLab (created on demand).
std::string userDataDir();
// Locates a shipped data directory ("profiles", "devices") next to the
// executable, in ../share/stickyaim, or in the source tree (developer builds).
// Returns an empty string if not found.
std::string findDataDir(const std::string& name, const std::string& sourceDirHint = {});

// Waits with sub-millisecond precision. Uses a high-resolution waitable timer
// (Windows 10 1803+) and, in hybrid mode, spins for the final stretch.
class PreciseWaiter {
public:
    PreciseWaiter();
    ~PreciseWaiter();
    PreciseWaiter(const PreciseWaiter&) = delete;
    PreciseWaiter& operator=(const PreciseWaiter&) = delete;

    bool highResolution() const noexcept { return highRes_; }
    // Sleep until the absolute steady-clock time `deadlineNs`. Spins for the
    // last `spinNs` nanoseconds (0 = pure sleep).
    void waitUntil(std::int64_t deadlineNs, std::int64_t spinNs) noexcept;

private:
    void* timer_ = nullptr;
    bool highRes_ = false;
};

// Raises the calling thread's scheduling priority for input processing.
// Returns a description of what was applied (for diagnostics).
std::string elevateCurrentThread(bool useMmcss);
void restoreCurrentThread();

// Requests 1 ms system timer resolution for the lifetime of the object
// (only needed when the high-resolution waitable timer is unavailable).
class TimerResolutionScope {
public:
    TimerResolutionScope();
    ~TimerResolutionScope();

private:
    bool active_ = false;
};

}  // namespace sa::win
