#include "stickyaim/platform/hidhide.hpp"

#include <windows.h>
#include <setupapi.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "stickyaim/platform/win_util.hpp"

namespace sa::win::hidhide {
namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string vidPid(std::uint16_t vid, std::uint16_t pid) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "VID_%04X&PID_%04X", vid, pid);
    return buf;
}

// Runs HidHideCLI with `args`, returns its exit code and stdout.
int runCli(const std::string& args, std::string* out) {
    const std::wstring cmd = L"\"" + fromUtf8(cliPath()) + L"\" " + fromUtf8(args);
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) return -1;
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    const BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writeEnd);
    if (!ok) {
        CloseHandle(readEnd);
        return -1;
    }
    std::string text;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(readEnd, buf, sizeof(buf), &n, nullptr) && n > 0) text.append(buf, n);
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(readEnd);
    if (out) *out = text;
    return static_cast<int>(code);
}

std::vector<std::string> hiddenList() {
    std::string text;
    runCli("--dev-list", &text);
    std::vector<std::string> out;
    size_t pos = 0;
    while ((pos = text.find("--dev-hide \"", pos)) != std::string::npos) {
        pos += 12;
        const size_t end = text.find('"', pos);
        if (end == std::string::npos) break;
        out.push_back(text.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

}  // namespace

std::string cliPath() {
    wchar_t pf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"ProgramW6432", pf, MAX_PATH);
    const std::filesystem::path base = n ? std::filesystem::path(pf) : std::filesystem::path(L"C:\\Program Files");
    const auto p = base / L"Nefarius Software Solutions" / L"HidHide" / L"x64" / L"HidHideCLI.exe";
    std::error_code ec;
    return std::filesystem::exists(p, ec) ? toUtf8(p.wstring()) : std::string();
}

bool installed() { return !cliPath().empty(); }

std::vector<std::string> controllerInstances(std::uint16_t vid, std::uint16_t pid) {
    std::vector<std::string> out;
    const std::string id = vidPid(vid, pid);
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) return out;
    SP_DEVINFO_DATA info{sizeof(info)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); ++i) {
        wchar_t instance[512] = {};
        if (!SetupDiGetDeviceInstanceIdW(set, &info, instance, 512, nullptr)) continue;
        wchar_t cls[128] = {};
        SetupDiGetDeviceRegistryPropertyW(set, &info, SPDRP_CLASS, nullptr, reinterpret_cast<PBYTE>(cls), sizeof(cls), nullptr);
        const std::string inst = toUtf8(instance);
        const std::string u = upper(inst);
        const std::string c = toUtf8(cls);
        // HID collections of the controller (not its keyboard/mouse collections) and its XInput interface.
        const bool hid = u.rfind("HID\\" + id, 0) == 0 && c == "HIDClass";
        const bool xusb = u.rfind("USB\\" + id, 0) == 0 && c == "XnaComposite";
        if (hid || xusb) out.push_back(inst);
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

int run(std::uint16_t vid, std::uint16_t pid, bool undo, const std::string& appExe, std::string* log) {
    if (!installed()) return 2;
    std::string l;
    const std::string id = vidPid(vid, pid);
    auto exec = [&](const std::string& args) {
        const int rc = runCli(args, nullptr);
        l += "HidHideCLI " + args + " -> " + std::to_string(rc) + "\n";
        return rc;
    };
    const std::vector<std::string> present = controllerInstances(vid, pid);
    for (const auto& entry : hiddenList()) {
        const std::string u = upper(entry);
        const bool ours = u.find(id) != std::string::npos;
        const bool virtualPad = u.find("VID_045E&PID_028E") != std::string::npos;
        // HidHide stores the paths as given; Windows reports them in upper case.
        const bool stillPresent =
            std::any_of(present.begin(), present.end(), [&](const std::string& p) { return upper(p) == u; });
        if (virtualPad || (ours && (undo || !stillPresent))) exec("--dev-unhide \"" + entry + "\"");
    }
    if (!undo) {
        if (present.empty()) {
            if (log) *log = l + "no present instances for " + id + "\n";
            return 3;
        }
        exec("--app-reg \"" + appExe + "\"");
        for (const auto& inst : present) exec("--dev-hide \"" + inst + "\"");
        exec("--cloak-on");
    }
    if (log) *log = l;
    return 0;
}

}  // namespace sa::win::hidhide
