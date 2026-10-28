#include "stickyaim/platform/system_probe.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>

#include "stickyaim/platform/win_util.hpp"

namespace sa::win {
namespace {

std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

bool readRegString(HKEY root, const wchar_t* key, const wchar_t* value, std::wstring& out) {
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return false;
    out = buf;
    return true;
}

bool keyExists(HKEY root, const wchar_t* key) {
    HKEY h;
    if (RegOpenKeyExW(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS) return false;
    RegCloseKey(h);
    return true;
}

// True if any subkey name of `key` starts with `prefix` (case-insensitive).
bool subkeyWithPrefix(HKEY root, const wchar_t* key, const std::wstring& prefix) {
    HKEY h;
    if (RegOpenKeyExW(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS) return false;
    const std::wstring p = lower(prefix);
    bool found = false;
    wchar_t name[512];
    for (DWORD i = 0;; ++i) {
        DWORD len = 512;
        if (RegEnumKeyExW(h, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        if (lower(name).rfind(p, 0) == 0) {
            found = true;
            break;
        }
    }
    RegCloseKey(h);
    return found;
}

}  // namespace

ServiceStatus queryService(const std::string& serviceName) {
    ServiceStatus s;
    s.name = serviceName;
    s.state = "not installed";
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        s.state = "unknown (" + lastErrorMessage(GetLastError()) + ")";
        return s;
    }
    const std::wstring wname = fromUtf8(serviceName);
    SC_HANDLE svc = OpenServiceW(scm, wname.c_str(), SERVICE_QUERY_STATUS);
    if (svc) {
        s.installed = true;
        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
            s.running = st.dwCurrentState == SERVICE_RUNNING;
            switch (st.dwCurrentState) {
                case SERVICE_RUNNING: s.state = "running"; break;
                case SERVICE_STOPPED: s.state = "stopped"; break;
                case SERVICE_START_PENDING: s.state = "starting"; break;
                case SERVICE_STOP_PENDING: s.state = "stopping"; break;
                default: s.state = "state " + std::to_string(st.dwCurrentState); break;
            }
        } else {
            s.state = "installed (status unavailable)";
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return s;
}

std::vector<std::string> runningProcesses(const std::vector<std::string>& exeNames) {
    std::vector<std::wstring> wanted;
    for (const auto& n : exeNames) wanted.push_back(lower(fromUtf8(n)));
    std::vector<std::string> found;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return found;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        const std::wstring exe = lower(pe.szExeFile);
        for (const auto& w : wanted)
            if (exe == w) {
                const std::string u = toUtf8(pe.szExeFile);
                if (std::find(found.begin(), found.end(), u) == found.end()) found.push_back(u);
            }
    }
    CloseHandle(snap);
    return found;
}

bool isProcessRunning(const std::string& exeName) { return !runningProcesses({exeName}).empty(); }

std::vector<LauncherStatus> probeLaunchers() {
    std::vector<LauncherStatus> out;

    {
        LauncherStatus s;
        s.id = "steam";
        s.name = "Steam";
        std::wstring path;
        if (readRegString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", path)) {
            s.installed = true;
            s.installPath = toUtf8(path);
            s.detection = "HKCU\\Software\\Valve\\Steam\\SteamPath";
        } else if (readRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath", path)) {
            s.installed = true;
            s.installPath = toUtf8(path);
            s.detection = "HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam\\InstallPath";
        } else {
            s.detection = "no Steam registry keys found";
        }
        s.running = isProcessRunning("steam.exe");
        s.notes = {
            "Steam Input is configured per game (Game > Properties > Controller). When enabled it reads the physical "
            "controller itself and presents its own virtual controller to the game.",
            "Steam Input can also pick up the Upgrade Aim virtual Xbox 360 pad. Test with Steam Input both enabled and "
            "disabled for the game and record which controller the game reacts to.",
            "If Steam Input is enabled, its own deadzones/curves stack on top of Upgrade Aim processing.",
        };
        out.push_back(std::move(s));
    }

    {
        LauncherStatus s;
        s.id = "battlenet";
        s.name = "Battle.net";
        std::wstring path;
        if (readRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Battle.net",
                          L"InstallLocation", path)) {
            s.installed = true;
            s.installPath = toUtf8(path);
            s.detection = "Uninstall\\Battle.net registry entry";
        } else if (keyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Blizzard Entertainment\\Battle.net")) {
            s.installed = true;
            s.detection = "HKLM\\SOFTWARE\\WOW6432Node\\Blizzard Entertainment\\Battle.net";
        } else {
            s.detection = "no Battle.net registry keys found";
        }
        s.running = !runningProcesses({"Battle.net.exe", "Agent.exe"}).empty();
        s.notes = {
            "Battle.net does not remap controllers itself; the game reads controllers directly.",
            "Record whether the game sees the physical pad, the virtual pad, or both (double input).",
        };
        out.push_back(std::move(s));
    }

    {
        LauncherStatus s;
        s.id = "xbox";
        s.name = "Xbox app / Microsoft Store";
        const bool pkg = subkeyWithPrefix(HKEY_CURRENT_USER,
                                          L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages",
                                          L"Microsoft.GamingApp_");
        s.installed = pkg;
        s.detection = pkg ? "Microsoft.GamingApp package registered for this user" : "Microsoft.GamingApp package not found";
        s.running = isProcessRunning("XboxPcApp.exe");
        s.notes = {
            "Store/Game Pass builds read controllers through XInput/GameInput like other PC builds.",
            "Store games run in a container; verify the virtual pad is visible to the game, not only to the Xbox app.",
        };
        out.push_back(std::move(s));
    }
    return out;
}

bool processIsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &e, sizeof(e), &size) != 0;
    CloseHandle(token);
    return ok && e.TokenIsElevated;
}

DriverReport probeDrivers() {
    DriverReport r;
    r.vigemBus = queryService("ViGEmBus");
    r.hidHide = queryService("HidHide");
    r.xusb = queryService("xusb22");
    r.isElevated = processIsElevated();
    return r;
}

}  // namespace sa::win
