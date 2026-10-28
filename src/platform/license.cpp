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

int dayNumber(int year, int month, int day) {
    using namespace std::chrono;
    const sys_days d = year_month_day{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                      std::chrono::day{static_cast<unsigned>(day)}};
    const sys_days epoch = year_month_day{std::chrono::year{2026}, January, std::chrono::day{1}};
    return static_cast<int>((d - epoch).count());
}

int today() {
    using namespace std::chrono;
    const sys_days now = floor<days>(system_clock::now());
    const sys_days epoch = year_month_day{std::chrono::year{2026}, January, std::chrono::day{1}};
    return static_cast<int>((now - epoch).count());
}

std::string dayToString(int day) {
    using namespace std::chrono;
    const year_month_day ymd{sys_days{year_month_day{std::chrono::year{2026}, January, std::chrono::day{1}}} + days{day}};
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02u/%02u/%04d", static_cast<unsigned>(ymd.day()), static_cast<unsigned>(ymd.month()),
                  static_cast<int>(ymd.year()));
    return buf;
}

Info verify(const std::string& code, const std::vector<std::uint8_t>& publicKey, const MachineHash& machine, int todayDay) {
    Info info;
    const auto bytes = decode(code);
    if (!bytes || bytes->size() != kPayload + kSignature) {
        info.error = "This is not a valid activation code.";
        return info;
    }
    if ((*bytes)[0] != 1) {
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
    info.expiryDay = b[2] | (b[3] << 8);
    info.serial = static_cast<std::uint32_t>(b[4] | (b[5] << 8) | (b[6] << 16) | (static_cast<std::uint32_t>(b[7]) << 24));
    if (info.machineBound && !std::equal(machine.begin(), machine.end(), b.begin() + 8)) {
        info.error = "This activation code was issued for a different computer.";
        return info;
    }
    if (info.expiryDay != 0 && todayDay > info.expiryDay) {
        info.error = "This activation code has expired.";
        return info;
    }
    info.valid = true;
    return info;
}

bool hasEmbeddedPublicKey() { return kLicensePublicKeySize > 0; }

Info verifyCode(const std::string& code) {
    const std::vector<std::uint8_t> pub(kLicensePublicKey, kLicensePublicKey + kLicensePublicKeySize);
    return verify(code, pub, machineHash(), today());
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
    return verifyCode(ss.str());
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

std::string issue(const std::vector<std::uint8_t>& privateKey, std::uint32_t serial, const std::optional<MachineHash>& machine,
                  int expiryDay, std::string* error) {
    std::vector<std::uint8_t> bytes(kPayload, 0);
    bytes[0] = 1;
    bytes[1] = machine ? 1 : 0;
    bytes[2] = static_cast<std::uint8_t>(expiryDay & 0xFF);
    bytes[3] = static_cast<std::uint8_t>((expiryDay >> 8) & 0xFF);
    for (int i = 0; i < 4; ++i) bytes[static_cast<size_t>(4 + i)] = static_cast<std::uint8_t>((serial >> (8 * i)) & 0xFF);
    if (machine) std::copy(machine->begin(), machine->end(), bytes.begin() + 8);
    std::uint8_t digest[32];
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
