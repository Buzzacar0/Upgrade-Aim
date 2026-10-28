#include "stickyaim/platform/vigem_sink.hpp"

#include <windows.h>

#include "stickyaim/core/logger.hpp"
#include "stickyaim/platform/xinput_source.hpp"

#if STICKYAIM_HAVE_VIGEM
#include <ViGEm/Client.h>
#endif

namespace sa::win {

bool ViGEmX360Sink::isCompiledIn() noexcept { return STICKYAIM_HAVE_VIGEM != 0; }

ViGEmX360Sink::ViGEmX360Sink(int physicalSlot) : physicalSlot_(physicalSlot) {}

ViGEmX360Sink::~ViGEmX360Sink() { close(); }

#if STICKYAIM_HAVE_VIGEM

namespace {

VOID CALLBACK rumbleCallback(PVIGEM_CLIENT, PVIGEM_TARGET, UCHAR large, UCHAR small, UCHAR, LPVOID user) {
    static_cast<ViGEmX360Sink*>(user)->onRumble(large, small);
}

std::string vigemError(VIGEM_ERROR e) {
    switch (e) {
        case VIGEM_ERROR_BUS_NOT_FOUND:
            return "ViGEmBus driver not found. Virtual controller output needs the ViGEmBus driver, which this "
                   "application does not install. See docs/DELIVERY.md.";
        case VIGEM_ERROR_NO_FREE_SLOT: return "no free virtual controller slot on the bus";
        case VIGEM_ERROR_BUS_VERSION_MISMATCH: return "ViGEmBus driver version is incompatible with this client";
        case VIGEM_ERROR_BUS_ACCESS_FAILED: return "could not open the ViGEmBus device (access failed)";
        case VIGEM_ERROR_TARGET_UNINITIALIZED: return "virtual target not initialized";
        default: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "ViGEm error 0x%08X", static_cast<unsigned>(e));
            return buf;
        }
    }
}

}  // namespace

bool ViGEmX360Sink::open(std::string& error) {
    close();
    auto client = vigem_alloc();
    if (!client) {
        error = "vigem_alloc failed (out of memory)";
        return false;
    }
    VIGEM_ERROR r = vigem_connect(client);
    if (!VIGEM_SUCCESS(r)) {
        error = vigemError(r);
        vigem_free(client);
        return false;
    }
    // Which XInput slots are taken before the virtual pad exists, so the slot
    // it really gets can be found by difference (see below).
    const auto before = XInputSource::querySlots();

    auto target = vigem_target_x360_alloc();
    r = vigem_target_add(client, target);
    if (!VIGEM_SUCCESS(r)) {
        error = vigemError(r);
        vigem_target_free(target);
        vigem_disconnect(client);
        vigem_free(client);
        return false;
    }
    client_ = client;
    target_ = target;

    // The slot ViGEm reports right after attaching is not reliable: on the
    // development machine it returned 0 while the physical pad held slot 0
    // and the virtual pad appeared in slot 1. The real slot is the one that
    // becomes connected after attaching, so wait for it (up to 1.5 s).
    int newSlot = -1;
    for (int i = 0; i < 75 && newSlot < 0; ++i) {
        Sleep(20);
        const auto after = XInputSource::querySlots();
        for (int s = 0; s < 4; ++s)
            if (after[static_cast<size_t>(s)].connected && !before[static_cast<size_t>(s)].connected) newSlot = s;
    }
    ULONG reported = 0;
    const bool haveReported = VIGEM_SUCCESS(vigem_target_x360_get_user_index(client, target, &reported));
    userIndex_.store(newSlot >= 0 ? newSlot : (haveReported ? static_cast<int>(reported) : -1));
    logInfo("vigem", "virtual X360 attached: new XInput slot " + std::to_string(newSlot) + ", ViGEm-reported index " +
                         (haveReported ? std::to_string(reported) : std::string("n/a")));

    // Feedback loop: the input source would be reading the virtual pad. That
    // only happens if the physical pad was not in its slot before attaching.
    const bool physicalWasThere = physicalSlot_ >= 0 && before[static_cast<size_t>(physicalSlot_)].connected;
    if (physicalSlot_ >= 0 && (!physicalWasThere || newSlot == physicalSlot_)) {
        error = "the physical controller is not in XInput slot " + std::to_string(physicalSlot_) +
                ", so the app would read its own virtual controller. Reconnect the controller, select it again on the "
                "Dashboard, then enable virtual output.";
        close();
        return false;
    }
    vigem_target_x360_register_notification(client, target, &rumbleCallback, this);
    return true;
}

void ViGEmX360Sink::close() noexcept {
    if (!client_) return;
    auto client = static_cast<PVIGEM_CLIENT>(client_);
    auto target = static_cast<PVIGEM_TARGET>(target_);
    XUSB_REPORT neutral;
    XUSB_REPORT_INIT(&neutral);
    vigem_target_x360_update(client, target, neutral);
    vigem_target_x360_unregister_notification(target);
    vigem_target_remove(client, target);
    vigem_target_free(target);
    vigem_disconnect(client);
    vigem_free(client);
    client_ = target_ = nullptr;
    userIndex_.store(-1);
    if (physicalSlot_ >= 0) XInputSource::setVibration(physicalSlot_, 0, 0);
    logInfo("vigem", "virtual X360 removed");
}

bool ViGEmX360Sink::submit(const RawGamepad& pad) noexcept {
    if (!client_) return false;
    XUSB_REPORT rep;
    rep.wButtons = pad.buttons;
    rep.bLeftTrigger = pad.leftTrigger;
    rep.bRightTrigger = pad.rightTrigger;
    rep.sThumbLX = pad.lx;
    rep.sThumbLY = pad.ly;
    rep.sThumbRX = pad.rx;
    rep.sThumbRY = pad.ry;
    return VIGEM_SUCCESS(vigem_target_x360_update(static_cast<PVIGEM_CLIENT>(client_), static_cast<PVIGEM_TARGET>(target_), rep));
}

#else  // !STICKYAIM_HAVE_VIGEM

bool ViGEmX360Sink::open(std::string& error) {
    error = "this build was compiled without ViGEm support (STICKYAIM_WITH_VIGEM=OFF)";
    return false;
}
void ViGEmX360Sink::close() noexcept {}
bool ViGEmX360Sink::submit(const RawGamepad&) noexcept { return false; }

#endif

void ViGEmX360Sink::onRumble(unsigned char large, unsigned char small) noexcept {
    if (forwardRumble_.load() && physicalSlot_ >= 0) XInputSource::setVibration(physicalSlot_, large, small);
}

}  // namespace sa::win
