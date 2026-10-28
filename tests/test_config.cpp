#include <gtest/gtest.h>

#include <filesystem>

#include <nlohmann/json.hpp>

#include "stickyaim/core/config_io.hpp"
#include "stickyaim/core/pipeline.hpp"

using namespace sa;
namespace fs = std::filesystem;

namespace {

fs::path tempDir() {
    const fs::path d = fs::temp_directory_path() / "stickyaim_tests";
    fs::create_directories(d);
    return d;
}

Profile richProfile() {
    Profile p;
    p.name = "Round trip";
    p.description = "unicode: ação ✓";
    p.hip.curveMode = CurveMode::PerAxis;
    p.hip.x.curve = {CurveType::Power, 1.7f};
    p.hip.y.curve = {CurveType::Custom, 1.0f, 0.0f, {{0.2f, 0.1f}, {0.6f, 0.55f}}};
    p.hip.x.fine = {true, 0.0f, 0.2f, 0.7f, 0.05f};
    p.hip.sensX = 1.2f;
    p.hip.sensY = 0.9f;
    p.hip.smoothing = {SmoothingType::OneEuro, 4.0f, 2.5f, 0.1f, 8.0f, 30.0f};
    p.ads.deadzone = {DeadzoneType::Axial, 0.07f, 0.92f};
    p.ads.radial.curve = {CurveType::Exponential, 1.0f, 2.5f};
    p.ads.radial.fast = {true, 0.75f, 1.0f, 1.8f, 0.1f};
    p.ads.antiDeadzone = 0.05f;
    p.ads.circularSaturation = false;
    p.contextTransitionMs = 12.0f;
    p.adsEnter = 0.2f;
    p.adsExit = 0.1f;
    p.leftTrigger = {0.05f, 0.95f};
    return p;
}

}  // namespace

TEST(Config, ProfileRoundTripIsLossless) {
    const Profile p = richProfile();
    const nlohmann::json j = toJson(p);
    const Profile back = profileFromJson(j);
    EXPECT_EQ(toJson(back), j);
    EXPECT_EQ(back.hip.y.curve.points.size(), 2u);
    EXPECT_EQ(back.hip.smoothing.type, SmoothingType::OneEuro);
    EXPECT_EQ(back.ads.deadzone.type, DeadzoneType::Axial);
    EXPECT_EQ(back.description, p.description);
}

TEST(Config, SaveAndLoadFile) {
    const auto path = (tempDir() / "profile_roundtrip.json").string();
    std::string err;
    ASSERT_TRUE(saveProfile(richProfile(), path, &err)) << err;
    Profile loaded;
    ASSERT_TRUE(loadProfile(path, loaded, &err)) << err;
    EXPECT_EQ(toJson(loaded), toJson(richProfile()));
    EXPECT_FALSE(fs::exists(path + ".tmp"));
}

TEST(Config, MissingFieldsKeepDefaults) {
    const Profile p = profileFromJson(nlohmann::json{{"name", "Sparse"}, {"hip", {{"sens_x", 1.5}}}});
    EXPECT_EQ(p.name, "Sparse");
    EXPECT_FLOAT_EQ(p.hip.sensX, 1.5f);
    EXPECT_FLOAT_EQ(p.hip.sensY, 1.0f);
    EXPECT_FLOAT_EQ(p.hip.deadzone.inner, DeadzoneConfig{}.inner);
    EXPECT_TRUE(validate(p).empty());
}

TEST(Config, RejectsInvalidFiles) {
    const auto dir = tempDir();
    std::string err;
    Profile out;
    ASSERT_TRUE(writeTextFile((dir / "garbage.json").string(), "{ not json"));
    EXPECT_FALSE(loadProfile((dir / "garbage.json").string(), out, &err));
    EXPECT_FALSE(err.empty());

    ASSERT_TRUE(writeTextFile((dir / "badtype.json").string(), R"({"name": 5})"));
    EXPECT_FALSE(loadProfile((dir / "badtype.json").string(), out, &err));

    ASSERT_TRUE(writeTextFile((dir / "invalid.json").string(), R"({"name": "x", "hip": {"deadzone": {"inner": 0.99}}})"));
    EXPECT_FALSE(loadProfile((dir / "invalid.json").string(), out, &err));
    EXPECT_NE(err.find("validate"), std::string::npos);

    EXPECT_FALSE(loadProfile((dir / "does_not_exist.json").string(), out, &err));
}

TEST(Config, CalibrationRoundTrip) {
    DeviceCalibration c;
    c.deviceKey = "3537:103D";
    c.right = {0.01f, -0.02f, 0.97f, 0.96f, 0.99f, 0.95f};
    const auto path = (tempDir() / "cal.json").string();
    ASSERT_TRUE(saveCalibration(c, path));
    DeviceCalibration back;
    ASSERT_TRUE(loadCalibration(path, back));
    EXPECT_EQ(back.deviceKey, c.deviceKey);
    EXPECT_FLOAT_EQ(back.right.centerY, -0.02f);
    EXPECT_FLOAT_EQ(back.right.maxNegY, 0.95f);
}

TEST(Config, ShippedExampleProfilesLoadAndCompile) {
    const fs::path dir = fs::path(STICKYAIM_SOURCE_DIR) / "profiles";
    ASSERT_TRUE(fs::exists(dir));
    int count = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() != ".json") continue;
        Profile p;
        std::string err;
        ASSERT_TRUE(loadProfile(e.path().string(), p, &err)) << e.path() << ": " << err;
        std::vector<std::string> errs;
        EXPECT_NE(CompiledProfile::compile(p, {}, &errs), nullptr) << e.path();
        ++count;
    }
    EXPECT_GE(count, 3);
}

// ------------------------------------------------------------- device database

#include "stickyaim/core/device_db.hpp"
#include "stickyaim/core/recording.hpp"

TEST(DeviceDb, ShippedDevicesLoad) {
    std::vector<std::string> errs;
    const auto db = loadDeviceDatabase((fs::path(STICKYAIM_SOURCE_DIR) / "devices").string(), &errs);
    EXPECT_TRUE(errs.empty()) << errs.front();
    const DeviceMode* mode = nullptr;
    const KnownDevice* t8k = matchDevice(db, 0x3537, 0x103D, &mode);
    ASSERT_NE(t8k, nullptr);
    EXPECT_EQ(t8k->name, "GameSir Tarantula 8K");
    ASSERT_NE(mode, nullptr);
    EXPECT_FALSE(mode->verification.empty());
    EXPECT_TRUE(t8k->hasHidMapping);
    EXPECT_FALSE(t8k->hidMapping.verified);
    EXPECT_EQ(matchDevice(db, 0x3537, 0xFFFF), nullptr);
}

TEST(DeviceDb, AxisSpecParsing) {
    HidAxisSource a;
    ASSERT_TRUE(parseAxisSpec("-Ry", a));
    EXPECT_EQ(a.usage, 0x34);
    EXPECT_TRUE(a.invert);
    EXPECT_EQ(a.kind, HidAxisSource::Kind::Full);
    ASSERT_TRUE(parseAxisSpec("Z+", a));
    EXPECT_EQ(a.kind, HidAxisSource::Kind::PositiveHalf);
    EXPECT_EQ(axisSpecToString(a), "Z+");
    ASSERT_TRUE(parseAxisSpec("none", a));
    EXPECT_EQ(a.kind, HidAxisSource::Kind::None);
    EXPECT_FALSE(parseAxisSpec("Wheel", a));
}

TEST(Recording, CsvRoundTrip) {
    std::vector<RawSample> in(3);
    in[0] = {100, 1, {Button::A, 10, 20, -32768, 32767, 5, -5}};
    in[1] = {250, 2, {0, 0, 255, 0, 0, 1000, -1000}};
    in[2] = {400, 4, {Button::Y | Button::Back, 1, 2, 3, 4, 5, 6}};
    const auto path = (tempDir() / "rec.csv").string();
    ASSERT_TRUE(saveRecording(in, path));
    std::vector<RawSample> out;
    std::string err;
    ASSERT_TRUE(loadRecording(path, out, &err)) << err;
    ASSERT_EQ(out.size(), in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        EXPECT_EQ(out[i].t, in[i].t);
        EXPECT_EQ(out[i].sequence, in[i].sequence);
        EXPECT_EQ(out[i].pad, in[i].pad);
    }
    ASSERT_TRUE(writeTextFile(path, "t_ns,sequence,buttons,lt,rt,lx,ly,rx,ry\n1,2,3\n"));
    EXPECT_FALSE(loadRecording(path, out, &err));
}
