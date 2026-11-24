#include "session/vetting.hpp"

#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "devices/tv.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine FaultFree(std::string_view machine) {
    auto loaded = LoadNetlistMachine(test::NetlistPath(machine));
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

VettingScript Script(std::string_view machine) {
    auto script = ReadVettingScriptFile(test::MachineDir(machine) / "puzzles/random.bwr");
    REQUIRE(script.has_value());
    return std::move(*script);
}

Vetting VettingOf(std::string_view machine) {
    auto vetting = Vetting::Make(FaultFree(machine), Script(machine));
    REQUIRE(vetting.has_value());
    REQUIRE(vetting->has_value());
    return std::move(**vetting);
}

// The Counter's Dark LED: the C19 Trace from U5 to D1 cracked.
const FaultDecl kDarkLed = CrackDecl{0, {"U5", "QD"}, {"D1", "1"}, 51};

Shown Picture(float value) {
    Shown shown;
    shown.locked = true;
    shown.cells.assign(static_cast<std::size_t>(kTvRows) * kTvColumns, value);
    return shown;
}

}  // namespace

TEST_CASE("vetting: the Counter's golden", "[session]") {
    const Vetting vetting = VettingOf("counter");
    CHECK_FALSE(vetting.watched().tv);
    REQUIRE(vetting.watched().lamps.size() == 1);
    CHECK(vetting.watched().lamps[0] == std::pair<std::string, std::size_t>{"D1", 0});
    // 250 ms, 500 ms, 750 ms, 1.25 s: dark, lit, lit, dark.
    REQUIRE(vetting.golden().size() == 4);
    const std::vector<float> expected{0, 1, 1, 0};
    for (std::size_t capture = 0; capture < 4; ++capture) {
        const Shown& shown = vetting.golden()[capture];
        CHECK(shown.cells.empty());
        CHECK(shown.lamps == std::vector<float>{expected[capture]});
    }
    CHECK(vetting.brightest() == 1.0f);
    CHECK(vetting.machine().completed());
}

TEST_CASE("vetting: the Dark LED's crack shows", "[session]") {
    Vetting vetting = VettingOf("counter");
    CHECK(vetting.shows(kDarkLed) == true);
    CHECK(vetting.shows(StuckAtDecl{0, "U3", "RCO", LogicValue::Zero}) == true);  // Frozen LED
    CHECK(vetting.shows(StuckAtDecl{0, "U1", "RCO", LogicValue::One}) == true);   // LED too fast
}

TEST_CASE("vetting: a stuck unconnected output does not", "[session]") {
    Vetting vetting = VettingOf("counter");
    // U2's QB goes nowhere; a load input tied low and stuck low changes nothing either.
    CHECK(vetting.shows(StuckAtDecl{0, "U2", "QB", LogicValue::Zero}) == false);
    CHECK(vetting.shows(StuckAtDecl{0, "U2", "A", LogicValue::Zero}) == false);
    // A Fault the Machine refuses does not show.
    CHECK(vetting.shows(StuckAtDecl{0, "U99", "QA", LogicValue::Zero}) == false);
    CHECK(vetting.shows(JointDecl{0, {"U5", "VCC"}}) == false);
}

TEST_CASE("vetting: the Machine is fault-free again", "[session]") {
    Vetting vetting = VettingOf("counter");
    REQUIRE(vetting.shows(kDarkLed) == true);
    CHECK(vetting.machine().completed());
    CHECK(vetting.machine().cracks().empty());
    // So the next Fault is vetted alone: one that shows nothing still shows nothing.
    CHECK(vetting.shows(StuckAtDecl{0, "U2", "QB", LogicValue::Zero}) == false);
    CHECK(vetting.shows(kDarkLed) == true);
}

TEST_CASE("vetting: a lost picture shows", "[session][slow]") {
    Vetting vetting = VettingOf("test_pattern");
    CHECK(vetting.watched().tv);
    CHECK(vetting.watched().lamps.empty());
    REQUIRE(vetting.golden().size() == 1);
    CHECK(vetting.golden()[0].locked);
    CHECK(vetting.golden()[0].cells.size() == static_cast<std::size_t>(kTvRows) * kTvColumns);
    CHECK(vetting.golden()[0].lamps.empty());
    CHECK(vetting.shows(StuckAtDecl{0, "X1", "OUT", LogicValue::Zero}) == true);  // no clock
    CHECK(vetting.shows(StuckAtDecl{0, "U12", "4Y", LogicValue::One}) == true);   // White screen
    CHECK(vetting.machine().completed());
}

TEST_CASE("vetting: one cell off does not", "[session]") {
    const Shown golden = Picture(0);
    Shown mine = Picture(0);
    CHECK_FALSE(Shows(golden, mine, 1));
    for (int i = 0; i < kShownCells - 1; ++i) {
        mine.cells[static_cast<std::size_t>(i) * 7] = 1;
    }
    CHECK_FALSE(Shows(golden, mine, 1));  // 99 cells
    mine.cells[5000] = 1;
    CHECK(Shows(golden, mine, 1));  // 100 cells
    // A cell within the margin is the same cell: an edge a fraction of a column away.
    Shown shifted = Picture(0);
    for (float& cell : shifted.cells) {
        cell = kCellMargin - 0.01f;
    }
    CHECK_FALSE(Shows(golden, shifted, 1));
    // The picture lost shows, whatever the cells.
    Shown lost = Picture(0);
    lost.locked = false;
    CHECK(Shows(golden, lost, 1));
    // Not the same outputs at all: it shows.
    CHECK(Shows(golden, Shown{}, 1));
}

TEST_CASE("vetting: lamps against the brightest", "[session]") {
    const auto lamps = [](std::vector<float> values) {
        Shown shown;
        shown.lamps = std::move(values);
        return shown;
    };
    // The BW-1's multiplexed segments reach 0.17: one of them dark shows.
    CHECK(Shows(lamps({0.17f, 0.17f}), lamps({0.17f, 0.0f}), 0.17f));
    CHECK_FALSE(Shows(lamps({0.17f, 0.17f}), lamps({0.17f, 0.14f}), 0.17f));
    // A fully lit lamp a tenth dimmer does not; a quarter dimmer does.
    CHECK_FALSE(Shows(lamps({1.0f}), lamps({0.9f}), 1));
    CHECK(Shows(lamps({1.0f}), lamps({0.7f}), 1));
    CHECK(Shows(lamps({0.0f}), lamps({0.28f}), 1));  // LED too fast: a glow where it was dark
}

TEST_CASE("vetting: a watched lamp the build lost reads dark", "[session]") {
    const Machine machine = FaultFree("counter");
    auto built = machine.build();
    REQUIRE(built.has_value());
    Watched watched = WatchedOf(**built);
    watched.lamps.emplace_back("D9", 0);  // as if a Fault had taken it off its Nets
    const auto shown = RunScript(**built, Script("counter"), watched, nullptr, 1, {});
    REQUIRE(shown.has_value());
    REQUIRE(shown->has_value());
    REQUIRE((*shown)->size() == 4);
    CHECK((**shown)[1].lamps == std::vector<float>{1, 0});
}

TEST_CASE("vetting: stops at the first capture time that shows", "[session]") {
    Vetting vetting = VettingOf("counter");
    Machine machine = FaultFree("counter");
    REQUIRE(machine.inject(kDarkLed).has_value());
    auto built = machine.build();
    REQUIRE(built.has_value());
    // Dark at 250 ms as the golden; dark at 500 ms where the golden is lit: no third capture.
    const auto shown = RunScript(**built, vetting.script(), vetting.watched(), &vetting.golden(),
                                 vetting.brightest(), {});
    REQUIRE(shown.has_value());
    REQUIRE(shown->has_value());
    REQUIRE((*shown)->size() == 2);
    CHECK_FALSE(Shows(vetting.golden()[0], (**shown)[0], 1));
    CHECK(Shows(vetting.golden()[1], (**shown)[1], 1));
}

TEST_CASE("vetting: stopped", "[session]") {
    std::stop_source source;
    source.request_stop();
    const auto stopped = Vetting::Make(FaultFree("counter"), Script("counter"), source.get_token());
    REQUIRE(stopped.has_value());
    CHECK_FALSE(stopped->has_value());

    Vetting vetting = VettingOf("counter");
    CHECK_FALSE(vetting.shows(kDarkLed, source.get_token()).has_value());
    CHECK(vetting.machine().completed());  // fault-free again all the same
    CHECK(vetting.shows(kDarkLed) == true);
}

TEST_CASE("vetting: an unknown switch in the script", "[session]") {
    VettingScript script = Script("counter");
    script.presses.push_back({"SW9", 1_ms, 2_ms});
    const auto vetting = Vetting::Make(FaultFree("counter"), std::move(script));
    REQUIRE_FALSE(vetting.has_value());
    CHECK(vetting.error() == "'SW9' is not a switch");
}

TEST_CASE("vetting: a golden with no picture is an error", "[session]") {
    VettingScript script = Script("test_pattern");
    script.captures = {1_ms};  // before the TV has seen a frame
    const auto vetting = Vetting::Make(FaultFree("test_pattern"), std::move(script));
    REQUIRE_FALSE(vetting.has_value());
    CHECK(vetting.error() == "the fault-free Machine has no picture at its capture 1 (1 ms)");
}
