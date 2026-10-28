#include "stickyaim/platform/license.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "stickyaim/platform/win_util.hpp"

namespace sa::win::license {
namespace {

#include "license_public_key.inc"  // kLicensePublicKey[] / kLicensePublicKeySize, written by upgradeaim-keygen --init

constexpr std::size_t kPayload = 16;
constexpr std::size_t kSignature = 64;
constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";  // Crockford base32
constexpr char kPrefix[] = "UA1";
constexpr std::int64_t kEpochUnix = 1767225600;  // 2026-01-01T00:00:00Z

bool sha256(const void* data, std::size_t size, std::uint8_t out[32]) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    const NTSTATUS st = BCryptHash(alg, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), out, 32);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

std::string encode(const std::vector<std::uint8_t>& bytes) {
    std::string out;
    unsigned buffer = 0;
    int bits = 0;
    for (std::uint8_t b : bytes) {
        buffer = (buffer << 8) | b;
        bits += 8;
        while (bits >= 5) {
            out += kAlphabet[(buffer >> (bits - 5)) & 31];
            bits -= 5;
        }
    }
    if (bits > 0) out += kAlphabet[(buffer << (5 - bits)) & 31];
    std::string grouped = kPrefix;
    for (std::size_t i = 0; i < out.size(); i += 5) grouped += "-" + out.substr(i, 5);
    return grouped;
}

std::optional<std::vector<std::uint8_t>> decode(const std::string& text) {
    std::string s;
    for (char c : text) {
        if (c == '-' || c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (c == 'O') c = '0';
        if (c == 'I' || c == 'L') c = '1';
        s += c;
    }
    if (s.rfind(kPrefix, 0) != 0) return std::nullopt;
    s = s.substr(std::char_traits<char>::length(kPrefix));
    std::vector<std::uint8_t> out;
    unsigned buffer = 0;
    int bits = 0;
    for (char c : s) {
        const char* p = std::strchr(kAlphabet, c);
        if (!p || c == '\0') return std::nullopt;
        buffer = (buffer << 5) | static_cast<unsigned>(p - kAlphabet);
        bits += 5;
        if (bits >= 8) {
            out.push_back(static_cast<std::uint8_t>((buffer >> (bits - 8)) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

BCRYPT_KEY_HANDLE importKey(const std::vector<std::uint8_t>& blob, bool isPrivate, BCRYPT_ALG_HANDLE* algOut) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    if (BCryptImportKeyPair(alg, nullptr, isPrivate ? BCRYPT_ECCPRIVATE_BLOB : BCRYPT_ECCPUBLIC_BLOB, &key,
                            const_cast<PUCHAR>(blob.data()), static_cast<ULONG>(blob.size()), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return nullptr;
    }
    *algOut = alg;
    return key;
}

}  // namespace

MachineHash machineHash() {
    MachineHash h{};
    wchar_t guid[128] = {};
    DWORD size = sizeof(guid);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
                 nullptr, guid, &size);
    const std::string text = "UpgradeAim/machine/" + toUtf8(guid);
    std::uint8_t digest[32];
    if (sha256(text.data(), text.size(), digest)) std::copy(digest, digest + 8, h.begin());
    return h;
}

std::string machineId() {
    const MachineHash h = machineHash();
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02X%02X-%02X%02X-%02X%02X-%02X%02X", h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
    return buf;
}

std::optional<MachineHash> parseMachineId(const std::string& text) {
    std::string hex;
    for (char c : text)
        if (std::isxdigit(static_cast<unsigned char>(c))) hex += c;
    if (hex.size() != 16) return std::nullopt;
    MachineHash h{};
    for (int i = 0; i < 8; ++i) h[static_cast<size_t>(i)] = static_cast<std::uint8_t>(std::stoul(hex.substr(static_cast<size_t>(i) * 2, 2), nullptr, 16));
    return h;
}

std::int64_t minuteNumber(int year, int month, int day, int hour, int minute) {
    using namespace std::chrono;
    const sys_days d = year_month_day{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                      std::chrono::day{static_cast<unsigned>(day)}};
    const sys_days epoch = year_month_day{std::chrono::year{2026}, January, std::chrono::day{1}};
    return static_cast<std::int64_t>((d - epoch).count()) * 1440 + hour * 60 + minute;
}

std::int64_t nowMinute() {
    using namespace std::chrono;
    return static_cast<std::int64_t>(duration_cast<minutes>(system_clock::now().time_since_epoch()).count()) - kEpochUnix / 60;
}

std::int64_t toUnixSeconds(std::int64_t minute) { return kEpochUnix + minute * 60; }

namespace {

std::uint32_t readU32(const std::vector<std::uint8_t>& b, size_t at) {
    return static_cast<std::uint32_t>(b[at] | (b[at + 1] << 8) | (b[at + 2] << 16) | (static_cast<std::uint32_t>(b[at + 3]) << 24));
}

// ---- per-machine state: highest time seen and serials of timed codes that
// already expired here. Kept in the registry and in a file, merged on read.
constexpr wchar_t kStateKey[] = L"Software\\UpgradeAim\\License";

std::filesystem::path stateFile() {
    wchar_t local[MAX_PATH] = {};
    GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    return std::filesystem::path(local) / L"UpgradeAim" / L"state.dat";
}

struct MachineState {
    std::int64_t lastSeen = 0;
    std::vector<std::uint32_t> expired;
};

void parseList(const std::string& text, std::vector<std::uint32_t>& out) {
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ','))
        if (!item.empty() && std::all_of(item.begin(), item.end(), ::isdigit)) out.push_back(static_cast<std::uint32_t>(std::stoul(item)));
}

MachineState loadState() {
    MachineState st;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStateKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        ULONGLONG seen = 0;
        DWORD size = sizeof(seen);
        if (RegQueryValueExW(key, L"LastSeen", nullptr, nullptr, reinterpret_cast<LPBYTE>(&seen), &size) == ERROR_SUCCESS)
            st.lastSeen = static_cast<std::int64_t>(seen);
        wchar_t list[4096] = {};
        size = sizeof(list) - sizeof(wchar_t);
        if (RegQueryValueExW(key, L"Expired", nullptr, nullptr, reinterpret_cast<LPBYTE>(list), &size) == ERROR_SUCCESS)
            parseList(toUtf8(list), st.expired);
        RegCloseKey(key);
    }
    std::ifstream f(stateFile());
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("seen=", 0) == 0) st.lastSeen = std::max<std::int64_t>(st.lastSeen, std::stoll(line.substr(5)));
        if (line.rfind("expired=", 0) == 0) parseList(line.substr(8), st.expired);
    }
    std::sort(st.expired.begin(), st.expired.end());
    st.expired.erase(std::unique(st.expired.begin(), st.expired.end()), st.expired.end());
    return st;
}

void saveState(const MachineState& st) {
    std::string list;
    for (auto s : st.expired) list += (list.empty() ? "" : ",") + std::to_string(s);
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kStateKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        const ULONGLONG seen = static_cast<ULONGLONG>(st.lastSeen);
        RegSetValueExW(key, L"LastSeen", 0, REG_QWORD, reinterpret_cast<const BYTE*>(&seen), sizeof(seen));
        const std::wstring w = fromUtf8(list);
        RegSetValueExW(key, L"Expired", 0, REG_SZ, reinterpret_cast<const BYTE*>(w.c_str()),
                       static_cast<DWORD>((w.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
    std::error_code ec;
    std::filesystem::create_directories(stateFile().parent_path(), ec);
    std::ofstream f(stateFile(), std::ios::trunc);
    f << "seen=" << st.lastSeen << "\nexpired=" << list << "\n";
}

}  // namespace

Info verify(const std::string& code, const std::vector<std::uint8_t>& publicKey, const MachineHash& machine, std::int64_t now) {
    Info info;
    const auto bytes = decode(code);
    if (!bytes || bytes->size() != kPayload + kSignature) {
        info.error = "This is not a valid activation code.";
        return info;
    }
    const std::uint8_t version = (*bytes)[0];
    if (version != 1 && version != 2) {
        info.error = "This activation code is for a different version of the app.";
        return info;
    }
    std::uint8_t digest[32];
    if (!sha256(bytes->data(), kPayload, digest)) {
        info.error = "Windows could not check the code (SHA-256 unavailable).";
        return info;
    }
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = publicKey.empty() ? nullptr : importKey(publicKey, false, &alg);
    if (!key) {
        info.error = "This build has no activation key configured.";
        return info;
    }
    const NTSTATUS st = BCryptVerifySignature(key, nullptr, digest, 32, const_cast<PUCHAR>(bytes->data() + kPayload),
                                              static_cast<ULONG>(kSignature), 0);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st != 0) {
        info.error = "This is not a valid activation code.";
        return info;
    }
    const auto& b = *bytes;
    info.machineBound = (b[1] & 1) != 0;
    if (version == 1) {
        // v1: expiry as a day number (valid through the end of that day), 32-bit serial.
        const int day = b[2] | (b[3] << 8);
        info.expiresMinute = day ? (static_cast<std::int64_t>(day) + 1) * 1440 : 0;
        info.serial = readU32(b, 4);
    } else {
        // v2: expiry as a minute, 16-bit serial.
        info.expiresMinute = readU32(b, 2);
        info.serial = static_cast<std::uint32_t>(b[6] | (b[7] << 8));
    }
    if (info.machineBound && !std::equal(machine.begin(), machine.end(), b.begin() + 8)) {
        info.error = "This activation code was issued for a different computer.";
        return info;
    }
    if (info.timed() && now >= info.expiresMinute) {
        info.expired = true;
        info.error = "This activation code has expired.";
        return info;
    }
    info.valid = true;
    return info;
}

bool hasEmbeddedPublicKey() { return kLicensePublicKeySize > 0; }

Info verifyCode(const std::string& code) {
    const std::vector<std::uint8_t> pub(kLicensePublicKey, kLicensePublicKey + kLicensePublicKeySize);
    return verify(code, pub, machineHash(), nowMinute());
}

Info evaluate(const std::string& code) {
    Info info = verifyCode(code);
    MachineState st = loadState();
    const std::int64_t now = nowMinute();
    bool dirty = false;
    if (info.timed()) {
        const bool burned = std::binary_search(st.expired.begin(), st.expired.end(), info.serial);
        if (info.expired && !burned) {
            st.expired.push_back(info.serial);
            std::sort(st.expired.begin(), st.expired.end());
            dirty = true;
        } else if (burned && info.valid) {
            info.valid = false;
            info.expired = true;
            info.error = "This test code has already expired on this computer.";
        } else if (info.valid && now + 10 < st.lastSeen) {
            // The clock is earlier than a time this PC already reached.
            info.valid = false;
            info.error = "The date and time of this PC were set back. Correct them to keep using Upgrade Aim.";
        }
    }
    if (now > st.lastSeen) {
        st.lastSeen = now;
        dirty = true;
    }
    if (dirty) saveState(st);
    return info;
}

std::string licensePath() { return userDataDir() + "\\license.key"; }

Info installedLicense() {
    std::ifstream f(std::filesystem::path(fromUtf8(licensePath())));
    if (!f) {
        Info i;
        i.error = "Not activated yet.";
        return i;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return evaluate(ss.str());
}
bool saveLicense(const std::string& code, std::string* error) {
    std::ofstream f(std::filesystem::path(fromUtf8(licensePath())), std::ios::trunc);
    if (!f) {
        if (error) *error = "could not write " + licensePath();
        return false;
    }
    f << code << "\n";
    return static_cast<bool>(f);
}

bool generateKeyPair(std::vector<std::uint8_t>& privateKey, std::vector<std::uint8_t>& publicKey, std::string* error) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    auto fail = [&](const char* what) {
        if (error) *error = what;
        if (key) BCryptDestroyKey(key);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    };
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return fail("ECDSA P-256 unavailable");
    if (BCryptGenerateKeyPair(alg, &key, 256, 0) != 0 || BCryptFinalizeKeyPair(key, 0) != 0) return fail("key generation failed");
    ULONG size = 0;
    BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &size, 0);
    privateKey.resize(size);
    if (BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB, privateKey.data(), size, &size, 0) != 0) return fail("export failed");
    publicKey = publicFromPrivate(privateKey);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return !publicKey.empty();
}

std::vector<std::uint8_t> publicFromPrivate(const std::vector<std::uint8_t>& privateKey) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = importKey(privateKey, true, &alg);
    if (!key) return {};
    ULONG size = 0;
    BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0);
    std::vector<std::uint8_t> pub(size);
    if (BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, pub.data(), size, &size, 0) != 0) pub.clear();
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return pub;
}

std::string issue(const std::vector<std::uint8_t>& privateKey, std::uint16_t serial, const MachineHash& machine,
                  std::int64_t expiresMinute, std::string* error) {
    if (expiresMinute < 0 || expiresMinute > 0xFFFFFFFFll) {
        if (error) *error = "expiry out of range";
        return {};
    }
    std::vector<std::uint8_t> bytes(kPayload, 0);
    bytes[0] = 2;
    bytes[1] = 1;  // always bound to one machine
    for (int i = 0; i < 4; ++i) bytes[static_cast<size_t>(2 + i)] = static_cast<std::uint8_t>((expiresMinute >> (8 * i)) & 0xFF);
    bytes[6] = static_cast<std::uint8_t>(serial & 0xFF);
    bytes[7] = static_cast<std::uint8_t>(serial >> 8);
    std::copy(machine.begin(), machine.end(), bytes.begin() + 8);    std::uint8_t digest[32];
    if (!sha256(bytes.data(), kPayload, digest)) {
        if (error) *error = "SHA-256 unavailable";
        return {};
    }
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = importKey(privateKey, true, &alg);
    if (!key) {
        if (error) *error = "invalid private key";
        return {};
    }
    std::uint8_t sig[kSignature];
    ULONG got = 0;
    const NTSTATUS st = BCryptSignHash(key, nullptr, digest, 32, sig, static_cast<ULONG>(kSignature), &got, 0);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st != 0 || got != kSignature) {
        if (error) *error = "signing failed";
        return {};
    }
    bytes.insert(bytes.end(), sig, sig + kSignature);
    return encode(bytes);
}

}  // namespace sa::win::license
