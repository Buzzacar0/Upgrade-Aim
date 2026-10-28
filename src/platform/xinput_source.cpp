#include "stickyaim/platform/xinput_source.hpp"

#include <windows.h>
#include <xinput.h>

#include "stickyaim/core/clock.hpp"

namespace sa::win {
namespace {

const char* subtypeName(BYTE s) {
    switch (s) {
        case XINPUT_DEVSUBTYPE_GAMEPAD: return "gamepad";
        case XINPUT_DEVSUBTYPE_WHEEL: return "wheel";
        case XINPUT_DEVSUBTYPE_ARCADE_STICK: return "arcade stick";
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK: return "flight stick";
        case XINPUT_DEVSUBTYPE_DANCE_PAD: return "dance pad";
        case XINPUT_DEVSUBTYPE_GUITAR:
        case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
        case XINPUT_DEVSUBTYPE_GUITAR_BASS: return "guitar";
        case XINPUT_DEVSUBTYPE_DRUM_KIT: return "drum kit";
        case XINPUT_DEVSUBTYPE_ARCADE_PAD: return "arcade pad";
        default: return "unknown";
    }
}

}  // namespace

XInputSource::XInputSource(int slot) : slot_(slot) {}

std::string XInputSource::name() const { return "XInput slot " + std::to_string(slot_); }

bool XInputSource::open() {
    XINPUT_STATE st{};
    havePacket_ = false;
    return XInputGetState(static_cast<DWORD>(slot_), &st) == ERROR_SUCCESS;
}

PollResult XInputSource::poll(RawSample& out) noexcept {
    XINPUT_STATE st;
    const DWORD r = XInputGetState(static_cast<DWORD>(slot_), &st);
    const TimestampNs t = nowNs();
    if (r != ERROR_SUCCESS) {
        havePacket_ = false;
        return PollResult::Disconnected;
    }
    if (havePacket_ && st.dwPacketNumber == lastPacket_) return PollResult::NoChange;
    havePacket_ = true;
    lastPacket_ = st.dwPacketNumber;
    out.t = t;
    out.sequence = st.dwPacketNumber;
    out.pad.buttons = st.Gamepad.wButtons;
    out.pad.leftTrigger = st.Gamepad.bLeftTrigger;
    out.pad.rightTrigger = st.Gamepad.bRightTrigger;
    out.pad.lx = st.Gamepad.sThumbLX;
    out.pad.ly = st.Gamepad.sThumbLY;
    out.pad.rx = st.Gamepad.sThumbRX;
    out.pad.ry = st.Gamepad.sThumbRY;
    return PollResult::NewSample;
}

std::array<XInputSlotInfo, 4> XInputSource::querySlots() {
    std::array<XInputSlotInfo, 4> out{};
    for (DWORD i = 0; i < 4; ++i) {
        auto& s = out[i];
        s.slot = static_cast<int>(i);
        XINPUT_STATE st{};
        s.connected = XInputGetState(i, &st) == ERROR_SUCCESS;
        if (!s.connected) continue;
        s.packet = st.dwPacketNumber;
        XINPUT_CAPABILITIES caps{};
        if (XInputGetCapabilities(i, 0, &caps) == ERROR_SUCCESS) {
            s.subtype = subtypeName(caps.SubType);
            s.wireless = (caps.Flags & XINPUT_CAPS_WIRELESS) != 0;
            s.forceFeedback = (caps.Flags & XINPUT_CAPS_FFB_SUPPORTED) != 0 || caps.Vibration.wLeftMotorSpeed != 0;
        }
    }
    return out;
}

void XInputSource::setVibration(int slot, std::uint8_t large, std::uint8_t small) noexcept {
    if (slot < 0 || slot > 3) return;
    XINPUT_VIBRATION v{static_cast<WORD>(large * 257), static_cast<WORD>(small * 257)};
    XInputSetState(static_cast<DWORD>(slot), &v);
}

}  // namespace sa::win
