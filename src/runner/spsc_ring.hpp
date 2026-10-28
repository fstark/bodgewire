#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

#include "sim/check.hpp"

namespace bw {

// Lock-free single-producer/single-consumer FIFO; capacity is a power of two.
template <class T>
class SpscRing {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    explicit SpscRing(std::size_t capacity) : slots_(capacity), mask_(capacity - 1) {
        Check(capacity >= 2 && std::has_single_bit(capacity),
              "spsc ring: capacity must be a power of two >= 2");
    }

    bool tryPush(const T& value) {  // producer; false if full
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == slots_.size()) {
            return false;
        }
        slots_[head & mask_] = value;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> tryPop() {  // consumer; nullopt if empty
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        const T value = slots_[tail & mask_];
        tail_.store(tail + 1, std::memory_order_release);
        return value;
    }

    bool empty() const {  // a snapshot, from either side
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    std::vector<T> slots_;
    std::size_t mask_;
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace bw
