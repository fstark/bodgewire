#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "sim/component.hpp"

namespace bw {

// The cells of a ROM: words of up to 8 bits, from a contents file. A stuck cell changes them.
class RomBlock {
public:
    explicit RomBlock(std::vector<std::uint8_t> words);
    std::size_t size() const { return words_.size(); }
    std::uint8_t operator[](std::size_t word) const { return words_[word]; }
    std::span<const std::uint8_t> words() const { return words_; }
    void stick(std::size_t word, unsigned bit, bool one);  // Check()s word < size(), bit < 8

private:
    std::vector<std::uint8_t> words_;
};

// The cells of a static RAM of `width` bits a word: random at power-on; a stuck bit is forced on
// every store and at power-on.
class RamBlock {
public:
    RamBlock(std::size_t size, unsigned width);
    void powerOn(PowerOnContext& ctx);  // `width` draws a word, bit 0 first, word 0 first
    std::size_t size() const { return words_.size(); }
    std::uint8_t operator[](std::size_t word) const { return words_[word]; }
    void store(std::size_t word, unsigned data);           // masked to `width` bits
    void stick(std::size_t word, unsigned bit, bool one);  // Check()s; forces the word at once

private:
    std::vector<std::uint8_t> words_, stuckMask_, stuckValue_;
    unsigned width_;
};

}  // namespace bw
