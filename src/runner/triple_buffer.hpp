#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace bw {

// Latest-value handoff between one writer and one reader; the reader never sees a torn value.
template <class T>
class TripleBuffer {
public:
    T& back() { return buffers_[back_]; }  // writer: fill, then publish()
    void publish() {
        const auto published = static_cast<std::uint8_t>(back_ | kFresh);
        back_ = middle_.exchange(published, std::memory_order_acq_rel) & kIndex;
    }

    bool update() {  // reader: true if a newer value was taken
        if ((middle_.load(std::memory_order_relaxed) & kFresh) == 0) {
            return false;
        }
        front_ = middle_.exchange(front_, std::memory_order_acq_rel) & kIndex;
        return true;
    }
    const T& front() const { return buffers_[front_]; }

private:
    static constexpr std::uint8_t kFresh = 0x4;  // published, not yet taken
    static constexpr std::uint8_t kIndex = 0x3;

    std::array<T, 3> buffers_{};
    std::uint8_t back_ = 0, front_ = 1;
    std::atomic<std::uint8_t> middle_{2};
};

}  // namespace bw
