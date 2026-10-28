#include "stickyaim/platform/win_util.hpp"

#include <windows.h>
#include <shlobj.h>

#include <filesystem>
#include <vector>
#include <avrt.h>
#include <immintrin.h>
#include <timeapi.h>

#include "stickyaim/core/clock.hpp"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace sa::win {

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string lastErrorMessage(unsigned long code) {
    wchar_t* buf = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&buf), 0, nullptr);
    std::wstring msg = n ? std::wstring(buf, n) : L"error " + std::to_wstring(code);
    if (buf) LocalFree(buf);
    while (!msg.empty() && (msg.back() == L'\n' || msg.back() == L'\r')) msg.pop_back();
    return toUtf8(msg);
}

std::string executableDir() {
    wchar_t buf[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    std::filesystem::path p(std::wstring(buf, n));
    return toUtf8(p.parent_path().wstring());
}

std::string userDataDir() {
    PWSTR roaming = nullptr;
    std::filesystem::path dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) dir = std::filesystem::path(roaming) / L"StickyAimLab";
    if (roaming) CoTaskMemFree(roaming);
    if (dir.empty()) dir = std::filesystem::path(fromUtf8(executableDir())) / L"userdata";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return toUtf8(dir.wstring());
}

std::string findDataDir(const std::string& name, const std::string& sourceDirHint) {
    namespace fs = std::filesystem;
    const fs::path exe(fromUtf8(executableDir()));
    const fs::path wname(fromUtf8(name));
    std::vector<fs::path> candidates = {exe / wname, exe / L".." / L"share" / L"stickyaim" / wname};
    if (!sourceDirHint.empty()) candidates.push_back(fs::path(fromUtf8(sourceDirHint)) / wname);
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::is_directory(c, ec)) return toUtf8(fs::weakly_canonical(c, ec).wstring());
    }
    return {};
}

PreciseWaiter::PreciseWaiter() {
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    highRes_ = timer_ != nullptr;
    if (!timer_) timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
}

PreciseWaiter::~PreciseWaiter() {
    if (timer_) CloseHandle(timer_);
}

void PreciseWaiter::waitUntil(std::int64_t deadlineNs, std::int64_t spinNs) noexcept {
    const std::int64_t sleepUntil = deadlineNs - spinNs;
    std::int64_t now = nowNs();
    if (timer_ && sleepUntil - now > 50'000) {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>((sleepUntil - now) / 100);  // relative, 100 ns units
        if (SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0)) WaitForSingleObject(timer_, INFINITE);
    }
    while (nowNs() < deadlineNs) _mm_pause();
}

namespace {
thread_local HANDLE g_mmcss = nullptr;
}

std::string elevateCurrentThread(bool useMmcss) {
    std::string applied;
    if (useMmcss) {
        DWORD idx = 0;
        g_mmcss = AvSetMmThreadCharacteristicsW(L"Games", &idx);
        if (g_mmcss) {
            AvSetMmThreadPriority(g_mmcss, AVRT_PRIORITY_HIGH);
            applied = "MMCSS 'Games' task, high priority";
        } else {
            applied = "MMCSS unavailable (" + lastErrorMessage(GetLastError()) + "); ";
        }
    }
    if (!g_mmcss) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        applied += "THREAD_PRIORITY_HIGHEST";
    }
    return applied;
}

void restoreCurrentThread() {
    if (g_mmcss) {
        AvRevertMmThreadCharacteristics(g_mmcss);
        g_mmcss = nullptr;
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}

TimerResolutionScope::TimerResolutionScope() { active_ = timeBeginPeriod(1) == TIMERR_NOERROR; }
TimerResolutionScope::~TimerResolutionScope() {
    if (active_) timeEndPeriod(1);
}

}  // namespace sa::win
