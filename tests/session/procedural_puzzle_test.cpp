#include "session/procedural_puzzle.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "machine/fault_draw.hpp"
#include "machine/machine_list.hpp"
#include "machine/repair.hpp"

using namespace bw;

namespace {

Machine FaultFree(const std::filesystem::path& netlist) {
    auto loaded = LoadNetlistMachine(netlist);
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

// A Fault's kind as DrawFault drew it: the variant's alternative, a column apart from a cell.
int KindOf(const FaultDecl& fault) {
    const auto* cell = std::get_if<CellDecl>(&fault);
    return static_cast<int>(fault.index()) * 2 + (cell != nullptr && !cell->word ? 1 : 0);
}

// The Repair a Fault's kind calls for.
bool Repair(Machine& machine, const FaultDecl& fault) {
    return std::visit(Overloaded{
                          [&](const StuckAtDecl& f) { return machine.swap(f.reference); },
                          [&](const CellDecl& f) { return machine.swap(f.pin.reference); },
                          [&](const SwitchDecl& f) { return machine.swap(f.reference); },
                          [&](const CrackDecl&) {
                              const Crack crack = machine.cracks().front();
                              return machine.mend(crack.layer, crack.at);
                          },
                          [&](const BridgeDecl& f) { return machine.removeBridge(f.a, f.b); },
                          [&](const JointDecl& f) { return machine.resolder(f.pin); },
                      },
                      fault);
}

}  // namespace

TEST_CASE("procedural puzzle: five of a kind at most, then another", "[session]") {
    // Nothing ever shows: kKindPicks kinds are picked, each tried kTriesAKind times, then the
    // error. No simulation: the test's own "does it show".
    const Machine dice = FaultFree(test::NetlistPath("dice"));
    std::vector<FaultDecl> asked;
    Rng rng(1);
    const auto made = MakeProceduralPuzzle(
        dice,
        [&](const FaultDecl& fault) {
            asked.push_back(fault);
            return std::optional(false);
        },
        rng);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error() == "no Fault shows on this Machine");
    REQUIRE(asked.size() == static_cast<std::size_t>(kKindPicks * kTriesAKind));
    std::vector<int> kinds;
    for (std::size_t pick = 0; pick < static_cast<std::size_t>(kKindPicks); ++pick) {
        const int kind = KindOf(asked[pick * kTriesAKind]);
        for (std::size_t same = 1; same < static_cast<std::size_t>(kTriesAKind); ++same) {
            CHECK(KindOf(asked[pick * kTriesAKind + same]) == kind);
        }
        kinds.push_back(kind);
        CHECK(dice.check(asked[pick * kTriesAKind]).has_value());
    }
    CHECK(std::ranges::count(kinds, kinds.front()) < kKindPicks);  // more than one kind picked
}

TEST_CASE("procedural puzzle: the first that shows is made", "[session]") {
    const Machine dice = FaultFree(test::NetlistPath("dice"));
    std::vector<FaultDecl> asked;
    Rng rng(1);
    const auto made = MakeProceduralPuzzle(
        dice,
        [&](const FaultDecl& fault) {
            asked.push_back(fault);
            return std::optional(asked.size() == 7);
        },
        rng);
    REQUIRE(made.has_value());
    REQUIRE(made->has_value());
    CHECK((*made)->tries == 7);
    REQUIRE(asked.size() == 7);
    CHECK(FormatFault((*made)->fault) == FormatFault(asked.back()));
}

TEST_CASE("procedural puzzle: stopped", "[session]") {
    const Machine dice = FaultFree(test::NetlistPath("dice"));
    int asked = 0;
    Rng rng(1);
    const auto made = MakeProceduralPuzzle(
        dice, [&](const FaultDecl&) { return ++asked == 3 ? std::nullopt : std::optional(false); },
        rng);
    REQUIRE(made.has_value());
    CHECK_FALSE(made->has_value());
    CHECK(asked == 3);
}

TEST_CASE("procedural puzzle: the same seed, the same Faults asked", "[session]") {
    const Machine dice = FaultFree(test::NetlistPath("dice"));
    const auto asked = [&](std::uint64_t seed) {
        std::vector<std::string> lines;
        Rng rng(seed);
        const auto made = MakeProceduralPuzzle(
            dice,
            [&](const FaultDecl& fault) {
                lines.push_back(FormatFault(fault));
                return std::optional(lines.size() == 20);
            },
            rng);
        REQUIRE(made.has_value());
        return lines;
    };
    CHECK(asked(5) == asked(5));
    CHECK(asked(5) != asked(6));
}

TEST_CASE("procedural puzzle: it shows and is not completed", "[session]") {
    const std::filesystem::path dir = test::MachineDir("counter");
    auto script = ReadVettingScriptFile(dir / "puzzles/random.bwr");
    REQUIRE(script.has_value());
    auto vetting = Vetting::Make(FaultFree(test::NetlistPath("counter")), std::move(*script));
    REQUIRE(vetting.has_value());
    REQUIRE(vetting->has_value());
    Rng rng(3);
    const auto made = MakeProceduralPuzzle(**vetting, rng);
    REQUIRE(made.has_value());
    REQUIRE(made->has_value());
    const FaultDecl fault = (*made)->fault;
    INFO(FormatFault(fault));
    CHECK((*made)->tries >= 1);
    CHECK((*vetting)->machine().completed());  // the Vetting's Machine is left fault-free
    CHECK((*vetting)->shows(fault) == true);   // and the Fault shows, again

    Machine machine = FaultFree(test::NetlistPath("counter"));
    REQUIRE(machine.inject(fault).has_value());
    CHECK_FALSE(machine.completed());
    REQUIRE(Repair(machine, fault));
    CHECK(machine.completed());

    Rng again(3);
    const auto same = MakeProceduralPuzzle(**vetting, again);
    REQUIRE(same.has_value());
    REQUIRE(same->has_value());
    CHECK(FormatFault((*same)->fault) == FormatFault(fault));
}

// Hidden (not listed, so not discovered as one test): tests/CMakeLists.txt adds one ctest a
// Machine, `procedural puzzle: NNN-name`, each running only its own section (`-c NNN-name`), so
// the Machines run in parallel. A Machine's Vetting is made once, then a Puzzle a seed: made, not
// completed, and completed after the Repair its kind calls for.
TEST_CASE("procedural puzzle: a Machine", "[.][session][slow]") {
    const MachineList list = ListMachines(test::RootDir() / "machines");
    for (const MachineEntry& entry : list.machines) {
        const std::string name = entry.dir.filename().string();
        DYNAMIC_SECTION(name) {
            const bool slowest = name == "130-bw2001" || name == "140-bw80";
            int seeds = slowest ? 1 : 3;
#ifndef NDEBUG
            // Debug is about 50 times slower: the Machines whose try is a tenth of a second in
            // release, one seed.
            const bool quick = name == "010-counter" || name == "020-led-chaser" ||
                               name == "030-dice" || name == "040-stopwatch" ||
                               name == "045-knob-meter" || name == "050-test-pattern" ||
                               name == "080-message";
            if (!quick) {
                SKIP("release only: a try on this Machine takes minutes in debug");
            }
            seeds = 1;
#endif
            REQUIRE(entry.random.has_value());
            auto script = ReadVettingScriptFile(*entry.random);
            REQUIRE(script.has_value());
            auto vetting = Vetting::Make(FaultFree(entry.netlist), std::move(*script));
            INFO((vetting ? std::string() : vetting.error()));
            REQUIRE(vetting.has_value());
            REQUIRE(vetting->has_value());
            for (int seed = 1; seed <= seeds; ++seed) {
                Rng rng(static_cast<std::uint64_t>(seed));
                const auto made = MakeProceduralPuzzle(**vetting, rng);
                INFO("seed " << seed << (made ? std::string() : ": " + made.error()));
                REQUIRE(made.has_value());
                REQUIRE(made->has_value());
                const FaultDecl fault = (*made)->fault;
                UNSCOPED_INFO(name << " seed " << seed << ": " << FormatFault(fault) << ", "
                                   << (*made)->tries << " tries");
                CHECK((*vetting)->machine().completed());
                Machine machine = FaultFree(entry.netlist);
                REQUIRE(machine.inject(fault).has_value());
                CHECK_FALSE(machine.completed());
                REQUIRE(Repair(machine, fault));
                CHECK(machine.completed());
            }
        }
    }
    SUCCEED();
}
