#include <gtest/gtest.h>

#include "stickyaim/platform/license.hpp"

namespace lic = sa::win::license;

namespace {

struct Keys {
    std::vector<std::uint8_t> priv, pub;
    Keys() {
        std::string err;
        EXPECT_TRUE(lic::generateKeyPair(priv, pub, &err)) << err;
    }
};

const lic::MachineHash kMachineA = {1, 2, 3, 4, 5, 6, 7, 8};
const lic::MachineHash kMachineB = {8, 7, 6, 5, 4, 3, 2, 1};

}  // namespace

TEST(License, PermanentCodeVerifies) {
    Keys k;
    std::string err;
    const std::string code = lic::issue(k.priv, 42, kMachineA, 0, &err);
    ASSERT_FALSE(code.empty()) << err;
    EXPECT_EQ(code.rfind("UA1-", 0), 0u);
    const auto info = lic::verify(code, k.pub, kMachineA, lic::nowMinute());
    EXPECT_TRUE(info.valid) << info.error;
    EXPECT_TRUE(info.machineBound);
    EXPECT_FALSE(info.timed());
    EXPECT_EQ(info.serial, 42u);
}

TEST(License, RejectsOtherMachineTamperingAndOtherKeys) {
    Keys k, other;
    const std::string code = lic::issue(k.priv, 7, kMachineA, 0, nullptr);
    EXPECT_FALSE(lic::verify(code, k.pub, kMachineB, 0).valid);
    EXPECT_FALSE(lic::verify(code, other.pub, kMachineA, 0).valid);
    std::string tampered = code;
    const size_t pos = tampered.size() / 2;
    tampered[pos] = tampered[pos] == 'A' ? 'B' : 'A';
    EXPECT_FALSE(lic::verify(tampered, k.pub, kMachineA, 0).valid);
    EXPECT_FALSE(lic::verify("UA1-HELLO", k.pub, kMachineA, 0).valid);
    EXPECT_FALSE(lic::verify(code, {}, kMachineA, 0).valid);
}

TEST(License, TimedCodeExpiresToTheMinute) {
    Keys k;
    const std::int64_t expires = lic::minuteNumber(2026, 10, 28, 18, 30);
    const std::string code = lic::issue(k.priv, 1, kMachineA, expires, nullptr);
    const auto before = lic::verify(code, k.pub, kMachineA, expires - 1);
    EXPECT_TRUE(before.valid) << before.error;
    EXPECT_TRUE(before.timed());
    EXPECT_EQ(before.expiresMinute, expires);
    const auto at = lic::verify(code, k.pub, kMachineA, expires);
    EXPECT_FALSE(at.valid);
    EXPECT_TRUE(at.expired);
    EXPECT_FALSE(lic::verify(code, k.pub, kMachineB, expires - 60).valid);  // still bound to the machine
}

TEST(License, CodeIsReadableAndForgiving) {
    Keys k;
    const std::string code = lic::issue(k.priv, 3, kMachineA, 0, nullptr);
    std::string lower;
    for (char c : code) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    EXPECT_TRUE(lic::verify("  " + lower + "\n", k.pub, kMachineA, 0).valid);  // case, spaces and newlines ignored
    EXPECT_EQ(lic::minuteNumber(2026, 1, 1), 0);
    EXPECT_EQ(lic::minuteNumber(2026, 1, 2, 1, 5), 1440 + 65);
    EXPECT_EQ(lic::toUnixSeconds(0), 1767225600);
    EXPECT_GT(lic::nowMinute(), lic::minuteNumber(2026, 10, 1));
    const auto parsed = lic::parseMachineId("0102-0304-0506-0708");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, kMachineA);
    EXPECT_FALSE(lic::parseMachineId("12-34").has_value());
}
