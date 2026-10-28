#pragma once

#include <array>
#include <string>

#include "stickyaim/core/io.hpp"

namespace sa::win {

struct XInputSlotInfo {
    int slot = -1;
    bool connected = false;
    std::string subtype;      // from XInputGetCapabilities
    bool wireless = false;
    bool forceFeedback = false;
    std::uint32_t packet = 0;
};

// Reads one XInput user slot (0-3) via XInputGetState (xinput1_4.dll).
//
// What this measures: the packet number increments whenever the driver
// reports a changed state, so NewSample is returned only on changes. Its rate
// is the rate of state changes visible through XInput, which can be lower than
// the USB polling rate (unchanged reports are not visible here) and is bounded
// by how often this source is polled.
class XInputSource final : public IInputSource {
public:
    explicit XInputSource(int slot);
    std::string name() const override;
    bool open() override;
    void close() override {}
    PollResult poll(RawSample& out) noexcept override;
    bool hasDeviceSequence() const noexcept override { return true; }
    int slot() const noexcept { return slot_; }

    static std::array<XInputSlotInfo, 4> querySlots();
    // Forwards rumble to the physical controller (used to pass game rumble
    // received by the virtual pad back to the real one).
    static void setVibration(int slot, std::uint8_t large, std::uint8_t small) noexcept;

private:
    int slot_;
    std::uint32_t lastPacket_ = 0;
    bool havePacket_ = false;
};

}  // namespace sa::win
