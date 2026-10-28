#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

namespace sa {

// Bounded single-producer / single-consumer lock-free ring buffer.
// push() and pop() never block and never allocate. Capacity is rounded up to a
// power of two. T must be trivially copyable so slots can be overwritten.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)  // padding from alignas is intentional (no false sharing)
#endif

template <typename T>
class SpscQueue {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    explicit SpscQueue(std::size_t capacity) {
        std::size_t cap = 2;
        while (cap < capacity) cap <<= 1;
        mask_ = cap - 1;
        buf_ = std::make_unique<T[]>(cap);
    }

    bool push(const T& v) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail > mask_) return false;  // full
        buf_[head & mask_] = v;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);
        if (tail == head) return false;
        out = buf_[tail & mask_];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    std::size_t sizeApprox() const noexcept {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
    }
    std::size_t capacity() const noexcept { return mask_ + 1; }

private:
    static constexpr std::size_t kLine = 64;
    alignas(kLine) std::atomic<std::size_t> head_{0};
    alignas(kLine) std::atomic<std::size_t> tail_{0};
    alignas(kLine) std::size_t mask_ = 0;
    std::unique_ptr<T[]> buf_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace sa
