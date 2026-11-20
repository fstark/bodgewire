#pragma once

#include <cstddef>
#include <string>

namespace bw {

// One instruction as text (P5t, P6p).
struct Disassembled {
    std::string text;    // upper case, numbers as $hex: "LD B,(IX-3)", "LDA ($FA),Y"
    std::size_t length;  // bytes taken
};

}  // namespace bw
