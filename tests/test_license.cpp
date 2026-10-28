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

TEST(License, IssuedCodeVerifies) {
    Keys k;
    std::string err;
    const std::string code = lic::issue(k.priv, 42, kMachineA, 0, &err);
    ASSERT_FALSE(code.empty()) << err;
    EXPECT_EQ(code.rfind("UA1-", 0), 0u);
    const auto info = lic::verify(code, k.pub, kMachineA, lic::today());
    EXPECT_TRUE(info.valid) << info.error;
    EXPECT_TRUE(info.machineBound);
    EXPECT_EQ(info.serial, 42u);
    EXPECT_EQ(info.expiryDay, 0);
}

TEST(License, RejectsOtherMachineTamperingAndOtherKeys) {
    Keys k, other;
    const std::string code = lic::issue(k.priv, 7, kMachineA, 0, nullptr);
    EXPECT_FALSE(lic::verify(code, k.pub, kMachineB, lic::today()).valid);
    EXPECT_FALSE(lic::verify(code, other.pub, kMachineA, lic::today()).valid);
    std::string tampered = code;
    const size_t pos = tampered.size() / 2;
    tampered[pos] = tampered[pos] == 'A' ? 'B' : 'A';
    EXPECT_FALSE(lic::verify(tampered, k.pub, kMachineA, lic::today()).valid);
    EXPECT_FALSE(lic::verify("UA1-HELLO", k.pub, kMachineA, lic::today()).valid);
    EXPECT_FALSE(lic::verify(code, {}, kMachineA, lic::today()).valid);
}

TEST(License, UnboundCodeWorksAnywhereAndExpires) {
    Keys k;
    const int expiry = lic::dayNumber(2026, 12, 31);
    const std::string code = lic::issue(k.priv, 1, std::nullopt, expiry, nullptr);
    EXPECT_TRUE(lic::verify(code, k.pub, kMachineB, expiry).valid);
    EXPECT_TRUE(lic::verify(code, k.pub, kMachineA, expiry - 100).valid);
    const auto late = lic::verify(code, k.pub, kMachineA, expiry + 1);
    EXPECT_FALSE(late.valid);
    EXPECT_EQ(late.expiryDay, expiry);
}

TEST(License, CodeIsReadableAndForgiving) {
    Keys k;
    const std::string code = lic::issue(k.priv, 3, kMachineA, 0, nullptr);
    std::string lower;
    for (char c : code) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    EXPECT_TRUE(lic::verify("  " + lower + "\n", k.pub, kMachineA, 0).valid);  // case, spaces and newlines ignored
    EXPECT_EQ(lic::dayNumber(2026, 1, 1), 0);
    EXPECT_EQ(lic::dayToString(lic::dayNumber(2027, 3, 5)), "05/03/2027");
    const auto parsed = lic::parseMachineId("0102-0304-0506-0708");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, kMachineA);
    EXPECT_FALSE(lic::parseMachineId("12-34").has_value());
}
