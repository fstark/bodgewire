#pragma once

#include <cstdint>

namespace bw {

using Time = std::int64_t;  // picoseconds

inline constexpr Time kPicosPerSecond = 1'000'000'000'000;

namespace literals {

constexpr Time operator""_ps(unsigned long long v) { return static_cast<Time>(v); }
constexpr Time operator""_ns(unsigned long long v) { return static_cast<Time>(v) * 1'000; }
constexpr Time operator""_us(unsigned long long v) { return static_cast<Time>(v) * 1'000'000; }
constexpr Time operator""_ms(unsigned long long v) {
    return static_cast<Time>(v) * 1'000'000'000;
}
constexpr Time operator""_s(unsigned long long v) { return static_cast<Time>(v) * kPicosPerSecond; }

}  // namespace literals

}  // namespace bw
