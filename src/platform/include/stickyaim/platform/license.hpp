#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Offline activation codes for Upgrade Aim.
//
// A code is 16 bytes of payload signed with ECDSA P-256 (Windows CNG):
//   [0] version (1)   [1] flags (bit 0: bound to one machine)
//   [2..3] last valid day (days since 2026-01-01, 0 = never expires)
//   [4..7] serial number   [8..15] machine hash (zero when not bound)
// followed by the 64-byte signature, Crockford-base32 encoded with dashes.
// The app embeds only the public key; codes are issued by the separate
// keygen tool, which holds the private key. Offline checks can be defeated
// by patching the executable; they stop casual sharing, not cracking.
namespace sa::win::license {

using MachineHash = std::array<std::uint8_t, 8>;

struct Info {
    bool valid = false;
    std::string error;          // why it is not valid (English; shown through T())
    bool machineBound = false;
    int expiryDay = 0;          // 0 = never
    std::uint32_t serial = 0;
};

MachineHash machineHash();          // SHA-256 of this PC's MachineGuid (salted), first 8 bytes
std::string machineId();            // machineHash() as XXXX-XXXX-XXXX-XXXX
std::optional<MachineHash> parseMachineId(const std::string& text);

int dayNumber(int year, int month, int day);  // days since 2026-01-01
int today();
std::string dayToString(int day);             // dd/mm/yyyy

// Verify against an explicit public key blob (BCRYPT_ECCPUBLIC_BLOB) / machine / date.
Info verify(const std::string& code, const std::vector<std::uint8_t>& publicKey, const MachineHash& machine, int todayDay);
// Verify with the public key compiled into this build, this machine and today.
Info verifyCode(const std::string& code);
bool hasEmbeddedPublicKey();

// Activation stored in %APPDATA%\UpgradeAim\license.key.
std::string licensePath();
Info installedLicense();
bool saveLicense(const std::string& code, std::string* error);

// ---- issuing (used by the keygen tool and the tests)
bool generateKeyPair(std::vector<std::uint8_t>& privateKey, std::vector<std::uint8_t>& publicKey, std::string* error);
std::vector<std::uint8_t> publicFromPrivate(const std::vector<std::uint8_t>& privateKey);
std::string issue(const std::vector<std::uint8_t>& privateKey, std::uint32_t serial, const std::optional<MachineHash>& machine,
                  int expiryDay, std::string* error);

}  // namespace sa::win::license
