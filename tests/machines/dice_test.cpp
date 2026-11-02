#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

// After a roll: the count N0..N2 as a number ('x' bits give -1) and D1..D7 as "1" lit / "0" dark.
struct Face {
    int count;
    std::string leds;
};

Face Read(const LoadedCircuit& loaded, const Simulator& sim) {
    Face face{0, ""};
    for (int bit = 0; bit < 3; ++bit) {
        const LogicValue v = sim.value(*PinNet(loaded, "U1", std::string("Q") + char('A' + bit)));
        if (v != Zero && v != One) {
            face.count = -1;
        } else if (face.count >= 0 && v == One) {
            face.count |= 1 << bit;
        }
    }
    for (char d = '1'; d <= '7'; ++d) {
        const NetId k = *PinNet(loaded, std::string("D") + d, "K");
        face.leds += sim.value(k) == Zero && !sim.isFloating(k) ? '1' : '0';
    }
    return face;
}

// Seed 1, run 1 ms, hold SW1 for `held`, run 100 µs more.
Face Roll(LoadedCircuit& loaded, Time held) {
    Button* button = ButtonOf(loaded, "SW1");
    REQUIRE(button != nullptr);
    button->setPressed(false);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(1_ms);
    button->setPressed(true);
    sim.wake(button->id());
    sim.runUntil(1_ms + held);
    button->setPressed(false);
    sim.wake(button->id());
    sim.runUntil(1_ms + held + 100_us);
    return Read(loaded, sim);
}

// LEDs D1..D7 of face n: TWO_N (D1, D7), FOUR_N (D2, D6), SIX_N (D3, D5), ODD_N (D4).
std::string Pattern(int n) {
    const char two = n >= 2 ? '1' : '0';
    const char four = n >= 4 ? '1' : '0';
    const char six = n == 6 ? '1' : '0';
    const char odd = n % 2 == 1 ? '1' : '0';
    return {two, four, six, odd, six, four, two};
}

}  // namespace

TEST_CASE("dice: netlist loads without warnings", "[machines]") {
    CHECK(DesignCircuit()->warnings.empty());
}

TEST_CASE("dice: six roll lengths give six faces", "[machines]") {
    const auto loaded = DesignCircuit();
    std::set<int> faces;
    for (int k = 0; k < 6; ++k) {
        const Face face = Roll(*loaded, 100_us + k * 1_us);
        INFO("k " << k << " count " << face.count << " leds " << face.leds);
        REQUIRE(face.count >= 1);
        REQUIRE(face.count <= 6);
        CHECK(face.leds == Pattern(face.count));
        faces.insert(face.count);
    }
    CHECK(faces == std::set<int>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("dice: the Board build rolls six faces", "[machines]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    std::set<int> faces;
    for (int k = 0; k < 6; ++k) {
        faces.insert(Roll(**built, 100_us + k * 1_us).count);
    }
    CHECK(faces == std::set<int>{1, 2, 3, 4, 5, 6});
}
