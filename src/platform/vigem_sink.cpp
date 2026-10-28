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

    ULONG idx = 0;
    // The user index is assigned asynchronously by the XInput stack; retry briefly.
    for (int i = 0; i < 50; ++i) {
        if (VIGEM_SUCCESS(vigem_target_x360_get_user_index(client, target, &idx))) {
            userIndex_.store(static_cast<int>(idx));
            break;
        }
        Sleep(10);
    }
    if (userIndex_.load() >= 0 && userIndex_.load() == physicalSlot_) {
        error = "virtual controller took the same XInput slot as the input source; refusing to create a feedback loop";
        close();
        return false;
    }
    vigem_target_x360_register_notification(client, target, &rumbleCallback, this);
    logInfo("vigem", "virtual X360 attached at XInput slot " + std::to_string(userIndex_.load()));
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
