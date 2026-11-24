#include "machine/fault_draw.hpp"

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"

using namespace bw;

namespace {

Machine FaultFree(std::string_view machine) {
    auto loaded = LoadNetlistMachine(test::NetlistPath(machine));
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

constexpr std::array kKinds{FaultKind::Stuck, FaultKind::Crack,  FaultKind::Bridge,
                            FaultKind::Joint, FaultKind::Column, FaultKind::Cell,
                            FaultKind::Switch};

}  // namespace

TEST_CASE("fault draw: each kind on a Machine that has it", "[machine]") {
    // The BW-1: chips, memories (one with two), switches. Each draw is of its kind and a Fault a
    // fresh Machine takes.
    const Machine machine = FaultFree("bw1");
    Rng rng(1);
    for (const FaultKind kind : kKinds) {
        for (int i = 0; i < 20; ++i) {
            const auto fault = DrawFault(machine, kind, rng);
            REQUIRE(fault.has_value());
            INFO(FormatFault(*fault));
            CHECK(machine.check(*fault).has_value());
            switch (kind) {
            case FaultKind::Stuck:
                CHECK(std::holds_alternative<StuckAtDecl>(*fault));
                break;
            case FaultKind::Crack:
                CHECK(std::holds_alternative<CrackDecl>(*fault));
                break;
            case FaultKind::Bridge:
                CHECK(std::holds_alternative<BridgeDecl>(*fault));
                break;
            case FaultKind::Joint:
                CHECK(std::holds_alternative<JointDecl>(*fault));
                break;
            case FaultKind::Column:
                REQUIRE(std::holds_alternative<CellDecl>(*fault));
                CHECK_FALSE(std::get<CellDecl>(*fault).word.has_value());
                break;
            case FaultKind::Cell:
                REQUIRE(std::holds_alternative<CellDecl>(*fault));
                CHECK(std::get<CellDecl>(*fault).word.has_value());
                break;
            case FaultKind::Switch:
                CHECK(std::holds_alternative<SwitchDecl>(*fault));
                break;
            }
        }
    }
    Machine taken = FaultFree("bw1");
    const auto fault = DrawFault(taken, FaultKind::Crack, rng);
    REQUIRE(fault.has_value());
    CHECK(taken.inject(*fault).has_value());
    CHECK_FALSE(taken.completed());
}

TEST_CASE("fault draw: a kind with nothing to act on", "[machine]") {
    const Machine counter = FaultFree("counter");  // no memory, no switch
    Rng rng(1);
    CHECK_FALSE(DrawFault(counter, FaultKind::Column, rng));
    CHECK_FALSE(DrawFault(counter, FaultKind::Cell, rng));
    CHECK_FALSE(DrawFault(counter, FaultKind::Switch, rng));
    CHECK(DrawFault(counter, FaultKind::Stuck, rng));
    for (int i = 0; i < 2000; ++i) {
        const auto kind = DrawKind(counter, rng);
        REQUIRE(kind.has_value());
        CHECK(*kind != FaultKind::Column);
        CHECK(*kind != FaultKind::Cell);
        CHECK(*kind != FaultKind::Switch);
    }
    const Machine dice = FaultFree("dice");  // a switch, no memory
    CHECK(DrawFault(dice, FaultKind::Switch, rng));
    CHECK_FALSE(DrawFault(dice, FaultKind::Cell, rng));
}

TEST_CASE("fault draw: the same seed, the same Fault", "[machine]") {
    const Machine machine = FaultFree("bw1");
    Rng first(7), again(7), other(8);
    bool differs = false;
    for (int i = 0; i < 50; ++i) {
        const auto kind = DrawKind(machine, first);
        REQUIRE(kind.has_value());
        CHECK(DrawKind(machine, again) == kind);
        const auto fault = DrawFault(machine, *kind, first);
        const auto same = DrawFault(machine, *kind, again);
        REQUIRE(fault.has_value());
        REQUIRE(same.has_value());
        CHECK(FormatFault(*fault) == FormatFault(*same));
        const auto otherKind = DrawKind(machine, other);
        const auto otherFault = DrawFault(machine, *otherKind, other);
        differs = differs || FormatFault(*otherFault) != FormatFault(*fault);
    }
    CHECK(differs);
}

TEST_CASE("fault draw: the kinds' shares", "[machine]") {
    const Machine machine = FaultFree("bw1");
    Rng rng(1);
    std::array<int, 7> drawn{};
    const int draws = 10000;
    for (int i = 0; i < draws; ++i) {
        ++drawn[static_cast<std::size_t>(*DrawKind(machine, rng))];
    }
    for (std::size_t kind = 0; kind < drawn.size(); ++kind) {
        INFO("kind " << kind);
        const double share = 100.0 * drawn[kind] / draws;
        CHECK(share > kFaultWeights[kind] - 2);
        CHECK(share < kFaultWeights[kind] + 2);
    }
    // No memory and no switch: the other four keep their proportions (35, 32, 12, 10 of 89).
    const Machine counter = FaultFree("counter");
    drawn = {};
    for (int i = 0; i < draws; ++i) {
        ++drawn[static_cast<std::size_t>(*DrawKind(counter, rng))];
    }
    CHECK(100.0 * drawn[0] / draws > 100.0 * 35 / 89 - 2);
    CHECK(100.0 * drawn[0] / draws < 100.0 * 35 / 89 + 2);
    CHECK(100.0 * drawn[3] / draws > 100.0 * 10 / 89 - 2);
    CHECK(100.0 * drawn[3] / draws < 100.0 * 10 / 89 + 2);
}

TEST_CASE("fault draw: every signal pin can be drawn", "[machine]") {
    // The Counter: every chip pin a stuck Fault may sit on comes out of 4000 draws, and no other.
    const Machine machine = FaultFree("counter");
    std::set<std::pair<std::string, std::string>> accepted;
    for (const LoadedPart& part : machine.designCircuit().parts) {
        const auto component = machine.designCircuit().components.find(part.reference);
        if (component == machine.designCircuit().components.end()) {
            continue;
        }
        for (std::size_t pin = 1; pin <= component->second->pins().size(); ++pin) {
            const StuckAtDecl stuck{0, part.reference, std::to_string(pin), LogicValue::Zero};
            if (part.reference.starts_with('U') && machine.check(stuck)) {
                accepted.emplace(part.reference, std::to_string(pin));
            }
        }
    }
    REQUIRE(accepted.size() == 70);  // five 74LS161: 14 signal pins each
    Rng rng(1);
    std::set<std::pair<std::string, std::string>> drawn;
    bool zero = false, one = false;
    for (int i = 0; i < 4000; ++i) {
        const auto fault = DrawFault(machine, FaultKind::Stuck, rng);
        REQUIRE(fault.has_value());
        const auto& stuck = std::get<StuckAtDecl>(*fault);
        if (stuck.reference.starts_with('U')) {
            drawn.emplace(stuck.reference, stuck.pin);
        }
        zero = zero || stuck.value == LogicValue::Zero;
        one = one || stuck.value == LogicValue::One;
    }
    CHECK(drawn == accepted);
    CHECK(zero);
    CHECK(one);
}
