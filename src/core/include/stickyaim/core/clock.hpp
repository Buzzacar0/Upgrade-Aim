#pragma once

#include <chrono>

#include "stickyaim/core/types.hpp"

namespace sa {

// High-resolution monotonic time. On MSVC std::chrono::steady_clock is backed
// by QueryPerformanceCounter, so this is QPC resolution (typically 100 ns).
inline TimestampNs nowNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

inline double nsToMs(TimestampNs ns) noexcept { return static_cast<double>(ns) / 1e6; }
inline double nsToUs(TimestampNs ns) noexcept { return static_cast<double>(ns) / 1e3; }

}  // namespace sa
