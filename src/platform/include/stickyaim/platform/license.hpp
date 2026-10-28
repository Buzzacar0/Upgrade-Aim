#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Offline activation codes for Upgrade Aim.
//
// A code is 16 bytes of payload signed with ECDSA P-256 (Windows CNG),
// Crockford-base32 encoded with dashes ("UA1-…"). Payload, version 2:
//   [0] 2   [1] flags (bit 0: bound to one machine; always set)
//   [2..5] expiry, minutes since 2026-01-01 00:00 UTC (0 = never)
//   [6..7] serial number   [8..15] machine hash
// Version 1 codes (expiry as a day number, 32-bit serial) are still accepted.
//
// The app embeds only the public key; codes are issued by the separate keygen
// tool, which holds the private key. Timed codes that expire on a PC are
// recorded there (registry + %LOCALAPPDATA%), so the same code stays locked
// even if the clock is moved back. Offline checks can be defeated by patching
// the executable or wiping that state; they stop casual sharing, not cracking.
namespace sa::win::license {

using MachineHash = std::array<std::uint8_t, 8>;

struct Info {
    bool valid = false;
    bool expired = false;
    std::string error;            // why it is not valid (English; shown through T())
    bool machineBound = false;
    std::int64_t expiresMinute = 0;  // minutes since 2026-01-01 UTC; 0 = never
    std::uint32_t serial = 0;
    bool timed() const { return expiresMinute != 0; }
};

MachineHash machineHash();          // SHA-256 of this PC's MachineGuid (salted), first 8 bytes
std::string machineId();            // machineHash() as XXXX-XXXX-XXXX-XXXX
std::optional<MachineHash> parseMachineId(const std::string& text);

std::int64_t minuteNumber(int year, int month, int day, int hour = 0, int minute = 0);  // UTC
std::int64_t nowMinute();
std::int64_t toUnixSeconds(std::int64_t minute);

// Signature, machine and expiry only (pure; used by the tests).
Info verify(const std::string& code, const std::vector<std::uint8_t>& publicKey, const MachineHash& machine, std::int64_t now);
// The same with the public key compiled into this build, this machine and now.
Info verifyCode(const std::string& code);
// verifyCode plus this PC's lock state: a timed code that already expired
// here stays rejected, and a clock set back before a time already seen is
// rejected. Updates that state.
Info evaluate(const std::string& code);
bool hasEmbeddedPublicKey();

// Activation stored in %APPDATA%\UpgradeAim\license.key.
std::string licensePath();
Info installedLicense();  // evaluate() of the stored code
bool saveLicense(const std::string& code, std::string* error);

// ---- issuing (used by the keygen tool and the tests)
bool generateKeyPair(std::vector<std::uint8_t>& privateKey, std::vector<std::uint8_t>& publicKey, std::string* error);
std::vector<std::uint8_t> publicFromPrivate(const std::vector<std::uint8_t>& privateKey);
std::string issue(const std::vector<std::uint8_t>& privateKey, std::uint16_t serial, const MachineHash& machine,
                  std::int64_t expiresMinute, std::string* error);

}  // namespace sa::win::license
