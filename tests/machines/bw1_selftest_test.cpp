// The BW-1 self-test (roms/bw1_003.asm, docs/p5k-bw1-monitor/BW1_MONITOR.md) on the KIM model:
// reached by the strap at reset or by 1800 GO, its steps, its failure codes, and no RAM used.

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "bw1_model.hpp"

using namespace bw;

namespace {

// Reset with the strap closed, then `us` of running.
Bw1Model& Strapped(Bw1Model& model, std::uint64_t us) {
    model.setStrap(true);
    model.reset();
    model.run(us);
    return model;
}

// The ROM's byte at `address` changed by `delta`.
void Patch(Bw1Model& model, std::uint16_t address, int delta) {
    const std::array<std::uint8_t, 1> byte{static_cast<std::uint8_t>(model.peek(address) + delta)};
    model.patchRom(address, byte);
}

}  // namespace

TEST_CASE("bw1 self-test: from the strap, every step to PASS", "[machines]") {
    const auto seed = GENERATE(1u, 2u, 3u);
    Bw1Model model(seed);
    Strapped(model, 2'000);
    CHECK(model.display(5'000) == "1     ");  // test 1 running
    model.run(300'000 - 7'000);
    CHECK(model.display() == "888888");  // every segment
    model.run(1'000'000 - 320'000);
    CHECK(model.display() == "8     ");  // digit 1 alone
    model.run(3'200'000 - 1'020'000);
    CHECK(model.display() == "PA55  ");  // PASS (S and 5 look the same)
    model.hold(kKeyGo);
    model.run(10'000);
    CHECK(model.display() == "PA5513");
    model.release();
    model.run(10'000);
    CHECK(model.display() == "PA55  ");
    CHECK_FALSE(model.timerTouched());
}

TEST_CASE("bw1 self-test: 1800 GO from the monitor", "[machines]") {
    Bw1Model model(1);
    model.run(20'000);
    model.type("AD 1800 GO");
    model.run(3'500'000);
    CHECK(model.display() == "PA55  ");
}

TEST_CASE("bw1 self-test: failure codes", "[machines]") {
    const auto seed = GENERATE(1u, 2u);
    SECTION("main RAM: $0234 bit 3") {
        Bw1Model model(seed);
        model.stick(0x0234, 3, true);  // pass A writes $34 ^ $55 = $61 there: bit 3 is 0
        CHECK(Strapped(model, 200'000).display() == "123408");
    }
    SECTION("002 RIOT RAM: $17D5 bit 0") {
        Bw1Model model(seed);
        model.stick(0x17D5, 0, true);  // pass A writes $15 ^ $A5 = $B0
        CHECK(Strapped(model, 200'000).display() == "27D501");
    }
    SECTION("003 RIOT RAM: $1795 bits 4-7") {
        Bw1Model model(seed);
        model.stick(0x1795, 7, false);  // the inverse of $B0's high nibble
        model.stick(0x1795, 6, true);
        model.stick(0x1795, 5, false);
        model.stick(0x1795, 4, false);
        CHECK(Strapped(model, 200'000).display() == "3795F0");
    }
    SECTION("002 ROM") {
        Bw1Model model(seed);
        Patch(model, 0x1D00, 1);
        CHECK(Strapped(model, 200'000).display() == "4C0001");
        Patch(model, 0x1D00, 0x0F);
        CHECK(Strapped(model, 200'000).display() == "4C0010");
    }
    SECTION("003 ROM") {
        Bw1Model model(seed);
        Patch(model, 0x1BF0, -1);  // past the code: an unwritten $FF
        CHECK(Strapped(model, 200'000).display() == "5800FF");
    }
}

TEST_CASE("bw1 self-test: dead RAM still reports", "[machines]") {
    Bw1Model model(3);
    for (unsigned address = 0; address < 0x400; ++address) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            model.stick(static_cast<std::uint16_t>(address), bit, true);
        }
    }
    for (unsigned address = 0x1780; address < 0x1800; ++address) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            model.stick(static_cast<std::uint16_t>(address), bit, true);
        }
    }
    CHECK(Strapped(model, 200'000).display() == "1000FF");  // page 0, $00 written, $FF read
}

TEST_CASE("bw1 self-test: the strap open, reset runs the monitor", "[machines]") {
    Bw1Model model(2);
    model.setStrap(false);
    model.reset();
    model.run(20'000);
    CHECK(model.display().substr(0, 4) == "0200");
}
