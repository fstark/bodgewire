#include "runner/meter_consumer.hpp"

#include <bit>
#include <cstdint>

namespace bw {

std::optional<double> MeterConsumer::reading() const {
    const std::optional<NetId> net = touched();
    const std::uint64_t p = published_.load(std::memory_order_acquire);
    if (!net || (p >> 32) != std::uint64_t{ToIndex(*net)} + 1) {
        return std::nullopt;
    }
    return std::bit_cast<float>(static_cast<std::uint32_t>(p));
}

void MeterConsumer::advanceTo(Time time) {
    const std::int64_t t = follow();
    state_.advanceTo(time);
    const auto reading = state_.reading();
    if (t < 0 || !reading) {
        published_.store(0, std::memory_order_release);
        return;
    }
    published_.store(static_cast<std::uint64_t>(t + 1) << 32 |
                         std::bit_cast<std::uint32_t>(static_cast<float>(*reading)),
                     std::memory_order_release);
}

}  // namespace bw
