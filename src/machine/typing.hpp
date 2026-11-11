#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "sim/time.hpp"

namespace bw {

inline constexpr Time kTypingInterval = 100'000'000'000;  // ps: 100 ms between keys

struct Typing {
    std::string reference;           // "J1"
    Time start;                      // the first key, simulated ps
    std::vector<std::uint8_t> keys;  // KeyboardCode values, one every kTypingInterval

    Time at(std::size_t key) const { return start + static_cast<Time>(key) * kTypingInterval; }
};

// "J1:200ms:HELLO\n": reference, start (ParseDuration), text after the second ':' (may hold
// ':'); \n Return, \b Backspace, \\ backslash. Error text otherwise.
std::expected<Typing, std::string> ParseTyping(std::string_view text);

}  // namespace bw
