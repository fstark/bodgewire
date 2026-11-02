#include "test_points.hpp"

#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "machine/machine.hpp"

namespace bw {

Crack PuzzleCrack(std::string_view puzzle) {
    const auto machine =
        LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(puzzle) + ".bwp"));
    REQUIRE(machine.has_value());
    REQUIRE(machine->cracks().size() == 1);
    return machine->cracks()[0];
}

Crack BoardCrack(std::string_view machine, const PinRef& a, const PinRef& b, double percent) {
    const auto loaded =
        LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / (std::string(machine) + ".bwn"));
    REQUIRE(loaded.has_value());
    const auto crack = loaded->placeCrack(a, b, percent);
    REQUIRE(crack.has_value());
    return *crack;
}

const Crack& H0Crack() {
    static const Crack crack = PuzzleCrack("broken_trace");
    return crack;
}

const Crack& C19Crack() {
    static const Crack crack = PuzzleCrack("dark_led");
    return crack;
}

const Crack& LedA5Crack() {
    static const Crack crack = PuzzleCrack("gap");
    return crack;
}

const Crack& SerCrack() {
    static const Crack crack = PuzzleCrack("dark_chaser");
    return crack;
}

const Crack& BtnNCrack() {
    static const Crack crack = PuzzleCrack("dead_button");
    return crack;
}

const Crack& N2Crack() {
    static const Crack crack = PuzzleCrack("loaded_dice");
    return crack;
}

const Crack& R1VccCrack() {
    static const Crack crack = BoardCrack("counter", {"R1", "1"}, {"U1", "16"}, 58);
    return crack;
}

const Crack& Sw1GndCrack() {
    static const Crack crack = BoardCrack("dice", {"SW1", "2"}, {"J1", "2"}, 7);
    return crack;
}

}  // namespace bw
