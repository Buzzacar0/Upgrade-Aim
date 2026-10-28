#pragma once

#include <atomic>
#include <string>

#include "stickyaim/core/io.hpp"

namespace sa::win {

// Virtual Xbox 360 controller through the ViGEmBus driver.
//
// Requirements (never installed or enabled by this application):
//  * ViGEmBus driver installed by the user (upstream is retired since 2023;
//    see docs/DELIVERY.md).
//  * To avoid the game seeing BOTH the physical and the virtual controller,
//    the physical device must be hidden from the game (e.g. HidHide with this
//    application allow-listed) or the game must be set to use one controller.
//
// Only compiled when STICKYAIM_WITH_VIGEM is ON; otherwise isCompiledIn()
// returns false and open() fails with an explanation.
class ViGEmX360Sink final : public IOutputSink {
public:
    // physicalSlot: XInput slot of the real controller, used to forward rumble
    // back to it and to refuse a configuration that would read our own output.
    explicit ViGEmX360Sink(int physicalSlot = -1);
    ~ViGEmX360Sink() override;

    static bool isCompiledIn() noexcept;

    std::string name() const override { return "ViGEm virtual Xbox 360 controller"; }
    bool open(std::string& error) override;
    void close() noexcept override;
    bool submit(const RawGamepad& pad) noexcept override;
    bool injectsIntoSystem() const noexcept override { return true; }
    int occupiedXInputSlot() const noexcept override { return userIndex_.load(); }

    void setForwardRumble(bool on) noexcept { forwardRumble_.store(on); }
    // Called from the ViGEm notification thread.
    void onRumble(unsigned char large, unsigned char small) noexcept;

private:
    void* client_ = nullptr;
    void* target_ = nullptr;
    int physicalSlot_;
    std::atomic<int> userIndex_{-1};
    std::atomic<bool> forwardRumble_{true};
};

}  // namespace sa::win
