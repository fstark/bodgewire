#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace bw {

// xoshiro256**, seeded through splitmix64; identical on every platform.
class Rng {
public:
    explicit Rng(std::uint64_t seed = 0) {
        for (auto& word : state_) {
            seed += 0x9e3779b97f4a7c15ULL;
            std::uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            word = z ^ (z >> 31);
        }
    }

    std::uint64_t next() {
        const std::uint64_t result = std::rotl(state_[1] * 5, 7) * 9;
        const std::uint64_t t = state_[1] << 17;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = std::rotl(state_[3], 45);
        return result;
    }

    bool nextBit() { return (next() >> 63) != 0; }

private:
    std::array<std::uint64_t, 4> state_{};
};

}  // namespace bw
