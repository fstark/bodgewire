#include "test_points.hpp"

#include <filesystem>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "machine/machine.hpp"
#include "parts/oscillator.hpp"

namespace bw {

Crack PuzzleCrack(std::string_view puzzle) {
    const auto machine = LoadPuzzleFile(test::PuzzlePath(puzzle));
    REQUIRE(machine.has_value());
    REQUIRE(machine->cracks().size() == 1);
    return machine->cracks()[0];
}

Time CanPeriod(const LoadedCircuit& loaded, std::string_view reference) {
    const auto it = loaded.components.find(reference);
    REQUIRE(it != loaded.components.end());
    const auto* oscillator = dynamic_cast<const Oscillator*>(it->second);
    REQUIRE(oscillator != nullptr);
    return oscillator->period();
}

Machine InlinePuzzle(std::string_view text) {
    const auto puzzle = ParsePuzzle(text, "inline.bwp");
    REQUIRE(puzzle.has_value());
    auto machine = LoadMachine(*puzzle, test::MachineDir(puzzle->machine));
    if (!machine) {
        for (const Diagnostic& d : machine.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

Crack BoardCrack(std::string_view machine, const PinRef& a, const PinRef& b, double percent) {
    const auto loaded = LoadNetlistMachine(test::NetlistPath(machine));
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

const Crack& VccR5R6Crack() {
    static const Crack crack = PuzzleCrack("half_life");
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

const Crack& ZipsCrack() {
    static const Crack crack = PuzzleCrack("zips");
    return crack;
}

const Crack& NineteenCrack() {
    static const Crack crack = PuzzleCrack("nineteen");
    return crack;
}

const Crack& BrokenBarCrack() {
    static const Crack crack = PuzzleCrack("broken_bar");
    return crack;
}

const Crack& NonStopCrack() {
    static const Crack crack = PuzzleCrack("non_stop");
    return crack;
}

const Crack& DarkSecondsCrack() {
    static const Crack crack = PuzzleCrack("dark_seconds");
    return crack;
}

const Crack& JumpingBallCrack() {
    static const Crack crack = PuzzleCrack("jumping_ball");
    return crack;
}

const Crack& DeadKnobCrack() {
    static const Crack crack = PuzzleCrack("dead_knob");
    return crack;
}

const Crack& SingleFiguresCrack() {
    static const Crack crack = PuzzleCrack("single_figures");
    return crack;
}

const Crack& LongPaddleCrack() {
    static const Crack crack = PuzzleCrack("long_paddle");
    return crack;
}

const Crack& InvisibleBallCrack() {
    static const Crack crack = PuzzleCrack("invisible_ball");
    return crack;
}

const Crack& GibberishCrack() {
    static const Crack crack = PuzzleCrack("gibberish");
    return crack;
}

const Crack& BlockLettersCrack() {
    static const Crack crack = PuzzleCrack("block_letters");
    return crack;
}

const Crack& LookalikesCrack() {
    static const Crack crack = PuzzleCrack("lookalikes");
    return crack;
}

const Crack& MIsReturnCrack() {
    static const Crack crack = PuzzleCrack("m_is_return");
    return crack;
}

const Crack& LightsOutCrack() {
    static const Crack crack = PuzzleCrack("lights_out");
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

const Crack& HalfSegmentCrack() {
    static const Crack crack = PuzzleCrack("half_segment");
    return crack;
}

const Crack& DeadDaCrack() {
    static const Crack crack = PuzzleCrack("dead_da");
    return crack;
}

const Crack& DeadRowCrack() {
    static const Crack crack = PuzzleCrack("dead_row");
    return crack;
}

const Crack& WrongSuspectCrack() {
    static const Crack crack = PuzzleCrack("wrong_suspect");
    return crack;
}

const Crack& HalfByteCrack() {
    static const Crack crack = PuzzleCrack("half_byte");
    return crack;
}

const Crack& StripedScreenCrack() {
    static const Crack crack = PuzzleCrack("striped_screen");
    return crack;
}

const Crack& RollingTextCrack() {
    static const Crack crack = PuzzleCrack("rolling_text");
    return crack;
}

const Crack& MissingKeysCrack() {
    static const Crack crack = PuzzleCrack("missing_keys");
    return crack;
}

const Crack& NoClockCrack() {
    static const Crack crack = PuzzleCrack("no_clock");
    return crack;
}

const Crack& BottomHalvesCrack() {
    static const Crack crack = PuzzleCrack("bottom_halves");
    return crack;
}

const Crack& EightKeysCrack() {
    static const Crack crack = PuzzleCrack("eight_keys");
    return crack;
}

const Crack& NoPrintCrack() {
    static const Crack crack = PuzzleCrack("no_print");
    return crack;
}

const Crack& RollingScreenCrack() {
    static const Crack crack = PuzzleCrack("rolling_screen");
    return crack;
}

const Crack& VerticalStripesCrack() {
    static const Crack crack = PuzzleCrack("vertical_stripes");
    return crack;
}

}  // namespace bw
