#include "parts/memory_block.hpp"

#include <utility>

#include "sim/check.hpp"

namespace bw {

RomBlock::RomBlock(std::vector<std::uint8_t> words) : words_(std::move(words)) {}

void RomBlock::stick(std::size_t word, unsigned bit, bool one) {
    Check(word < words_.size() && bit < 8, "rom: no such cell");
    const auto mask = static_cast<std::uint8_t>(1u << bit);
    words_[word] = static_cast<std::uint8_t>(one ? words_[word] | mask : words_[word] & ~mask);
}

RamBlock::RamBlock(std::size_t size, unsigned width)
    : words_(size), stuckMask_(size), stuckValue_(size), width_(width) {
    Check(width >= 1 && width <= 8, "ram: width must be 1 to 8 bits");
}

void RamBlock::powerOn(PowerOnContext& ctx) {
    for (std::uint8_t& word : words_) {
        word = 0;
        for (unsigned bit = 0; bit < width_; ++bit) {
            word = static_cast<std::uint8_t>(word | (ctx.randomBit() ? 1u << bit : 0u));
        }
    }
    for (std::size_t w = 0; w < words_.size(); ++w) {
        store(w, words_[w]);
    }
}

void RamBlock::store(std::size_t word, unsigned data) {
    words_[word] = static_cast<std::uint8_t>(
        ((data & ~unsigned{stuckMask_[word]}) | stuckValue_[word]) & ((1u << width_) - 1));
}

void RamBlock::stick(std::size_t word, unsigned bit, bool one) {
    Check(word < words_.size() && bit < width_, "ram: no such cell");
    const auto mask = static_cast<std::uint8_t>(1u << bit);
    stuckMask_[word] |= mask;
    stuckValue_[word] =
        static_cast<std::uint8_t>(one ? stuckValue_[word] | mask : stuckValue_[word] & ~mask);
    store(word, words_[word]);
}

}  // namespace bw
