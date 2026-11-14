#include "parts/memory_block.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/simulator.hpp"

using namespace bw;

namespace {

// A RAM block of 16 words of 4 bits powered on with the Simulator, no Pins.
class BlockHolder final : public Component {
public:
    explicit BlockHolder(std::string name) : Component(std::move(name), {}) {}
    void powerOn(PowerOnContext& ctx) override { block.powerOn(ctx); }
    void evaluate(EvalContext&) override {}
    RamBlock block{16, 4};
};

// The 2114's power-on loop before P5j, written out: the draws the block must repeat.
class OldLoop final : public Component {
public:
    explicit OldLoop(std::string name) : Component(std::move(name), {}) {}
    void powerOn(PowerOnContext& ctx) override {
        for (std::uint8_t& word : words) {
            word = 0;
            for (int bit = 0; bit < 4; ++bit) {
                word = static_cast<std::uint8_t>(word | (ctx.randomBit() ? 1u << bit : 0u));
            }
        }
    }
    void evaluate(EvalContext&) override {}
    std::array<std::uint8_t, 16> words{};
};

}  // namespace

TEST_CASE("memory block: RAM power-on draws as the 2114 did", "[parts]") {
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        Circuit a;
        auto& holder = a.add<BlockHolder>("U1");
        a.finalize();
        Simulator simA(a);
        simA.powerOn(seed);
        Circuit b;
        auto& old = b.add<OldLoop>("U1");
        b.finalize();
        Simulator simB(b);
        simB.powerOn(seed);
        for (std::size_t w = 0; w < 16; ++w) {
            INFO(seed << " " << w);
            CHECK(holder.block[w] == old.words[w]);
        }
    }
}

TEST_CASE("memory block: RAM stores masked to its width, stuck bits forced", "[parts]") {
    RamBlock block(8, 4);
    block.store(1, 0xFF);
    CHECK(block[1] == 0x0F);
    block.stick(1, 2, false);
    CHECK(block[1] == 0x0B);  // forced at once
    block.store(1, 0x0F);
    CHECK(block[1] == 0x0B);
    block.stick(2, 0, true);
    block.store(2, 0x00);
    CHECK(block[2] == 0x01);
    block.store(3, 0x00);
    CHECK(block[3] == 0x00);  // other words untouched
    CHECK_THROWS_AS(block.stick(8, 0, true), std::logic_error);
    CHECK_THROWS_AS(block.stick(0, 4, true), std::logic_error);
    CHECK_THROWS_AS(RamBlock(4, 9), std::logic_error);
}

TEST_CASE("memory block: a stuck RAM bit survives power-on", "[parts]") {
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        Circuit circuit;
        auto& holder = circuit.add<BlockHolder>("U1");
        holder.block.stick(5, 3, true);
        holder.block.stick(6, 0, false);
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(seed);
        CHECK((holder.block[5] & 0x8) != 0);
        CHECK((holder.block[6] & 0x1) == 0);
    }
}

TEST_CASE("memory block: a stuck ROM bit changes the word", "[parts]") {
    RomBlock block(std::vector<std::uint8_t>{0x00, 0xFF, 0x5A});
    CHECK(block.size() == 3);
    block.stick(0, 7, true);
    block.stick(1, 0, false);
    CHECK(block[0] == 0x80);
    CHECK(block[1] == 0xFE);
    CHECK(block[2] == 0x5A);
    CHECK(block.words()[1] == 0xFE);
    CHECK_THROWS_AS(block.stick(3, 0, true), std::logic_error);
    CHECK_THROWS_AS(block.stick(0, 8, true), std::logic_error);
}
