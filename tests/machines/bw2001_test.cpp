#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../paths.hpp"
#include "bw2001_harness.hpp"
#include "bw2001_rig.hpp"
#include "bw2001_screen.hpp"
#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "netlist/rom_directory.hpp"
#include "parts/matrix_keyboard.hpp"
#include "parts/ram2114.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::string NetlistText() {
    std::ifstream in(Bw2001NetlistPath());
    REQUIRE(in);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// The netlist edited as text (a test's variant: DIAG grounded), on our firmware, built.
std::unique_ptr<LoadedCircuit> VariantCircuit(const std::string& text) {
    auto netlist = ParseNetlist(text, "bw2001.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, Bw2001NetlistPath().parent_path());
    REQUIRE(ApplyRomDirectory(*netlist, test::DebugRomDir("bw2001")).has_value());
    auto result = BuildCircuit(*netlist);
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    CHECK((*result)->warnings.empty());
    return std::move(*result);
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(Bw2001NetlistPath());
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

}  // namespace

TEST_CASE("bw2001 machine: the netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("bw2001 machine: stage 1, the TV locks on 260 lines at 60 Hz", "[machines][slow]") {
    Running m(1, Bw2001BoardCircuit());
    const NetId hdrive = *ConnectorNet(*m.loaded, "MONITOR", "HDRIVE");
    const NetId vdrive = *ConnectorNet(*m.loaded, "MONITOR", "VDRIVE");
    m.sim->record(hdrive);
    m.sim->record(vdrive);
    m.sim->runUntil(150_ms);  // the counters fall into step within a few frames of power-on
    m.tv->update();
    CHECK(m.tv->tv().isLocked());
    CHECK(m.tv->tv().completedStats().lines == 260);
    // HORZ DRIVE: high 24 us of every 64; VERT DRIVE: low 20 lines of every 260.
    const auto edges = [&](NetId net, LogicValue to) {
        std::vector<Time> at;
        for (const Sample& s : m.sim->recorder().samples(net)) {
            if (s.value == to && s.time > 100_ms) {
                at.push_back(s.time);
            }
        }
        return at;
    };
    const auto hRise = edges(hdrive, LogicValue::One), hFall = edges(hdrive, LogicValue::Zero);
    REQUIRE(hRise.size() > 10);
    CHECK(hRise[2] - hRise[1] == 64_us);  // one clock: both edges through the same gates
    const auto fall = std::ranges::upper_bound(hFall, hRise[1]);
    REQUIRE(fall != hFall.end());
    CHECK(*fall - hRise[1] > 24_us - 100'000);
    CHECK(*fall - hRise[1] < 24_us + 100'000);
    const auto vFall = edges(vdrive, LogicValue::Zero), vRise = edges(vdrive, LogicValue::One);
    REQUIRE(vFall.size() >= 2);
    CHECK(vFall[1] - vFall[0] == 260 * 64_us);
    const auto up = std::ranges::upper_bound(vRise, vFall[0]);
    REQUIRE(up != vRise.end());
    CHECK(*up - vFall[0] > 20 * 64_us - 100'000);  // within the gates' delays
    CHECK(*up - vFall[0] < 20 * 64_us + 100'000);
}

TEST_CASE("bw2001 machine: stage 1, the picture is the video RAM", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1u, 2u, 3u);
    INFO("seed " << seed);
    Running m(seed, Bw2001BoardCircuit());
    m.runUntil(150_ms);
    REQUIRE(m.tv->tv().isLocked());
    const TvFrame& frame = m.tv->tv().completedFrame();
    CHECK(Bw2001BorderDots(frame) == 0);
    int wrong = 0;
    for (int cell = 0; cell < kBw2001Columns * kBw2001Rows; ++cell) {
        if (Bw2001CellDots(frame, cell) != Bw2001Glyph(m.screen(static_cast<std::size_t>(cell)))) {
            if (++wrong <= 3) {
                UNSCOPED_INFO("cell " << cell << " byte " << int(m.screen(cell)));
            }
        }
    }
    CHECK(wrong == 0);
}

TEST_CASE("bw2001 machine: stage 2, the debug ROMs' banner on the TV, no snow",
          "[machines][slow]") {
    Running m(1, Bw2001DebugCircuit(), true);
    const NetId led = m.loaded->nets.at("DIAG_LED_N");
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine(3'000'000));
    UNSCOPED_INFO("READY at " << m.sim->now() / 1_ms << " ms");
    CHECK(typist.rows() == kBw2001Banner);
    m.runUntil(m.sim->now() + 40_ms);  // two whole frames after the last print
    CHECK(Bw2001TvRows(m.tv->tv().completedFrame()) == kBw2001Banner);
    CHECK(Bw2001BorderDots(m.tv->tv().completedFrame()) == 0);
    CHECK(m.snow->all > 1000);     // the clear (1000 bytes) and the banner: the watch sees them
    CHECK(m.snow->accesses == 0);  // the editor touches the screen only in the retrace
    CHECK(m.sim->value(led) == LogicValue::One);  // DS1 dark: no software selects row 11
}

TEST_CASE("bw2001 machine: stage 3, typing through the keyboard connector", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.3 s simulated: minutes in debug; runs in the release build");
#endif
    Running m(2, Bw2001DebugCircuit());
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine());
    Bw2001Type(m, "PRINT 2+2\r");
    REQUIRE(typist.waitForLine());
    const auto rows = typist.rows();
    REQUIRE(rows.size() >= 3);
    CHECK(rows[rows.size() - 3] == "PRINT 2+2");
    CHECK(rows[rows.size() - 2] == "4");
    CHECK(rows[rows.size() - 1] == "READY");
    m.runUntil(m.sim->now() + 40_ms);
    CHECK(Bw2001TvRows(m.tv->tv().completedFrame()) == rows);
}

TEST_CASE("bw2001 machine: stage 3, a scrolling screen without snow", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Running m(3, Bw2001DebugCircuit(), true);
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine());
    // From the banner: before reset ends (1 ms) the 6502's address is its power-on state, which
    // may select the screen for a moment (seed 3 since UH2 and UH6 were fitted, P5q2).
    const int before = m.snow->accesses;
    Bw2001Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r");
    REQUIRE(typist.waitForLine(10'000'000));
    const auto rows = typist.rows();
    REQUIRE(rows.size() == 24);  // scrolled: the cursor's row is the 25th
    CHECK(rows[21] == "29");
    CHECK(rows[22] == "30");
    CHECK(rows[23] == "READY");
    CHECK(m.snow->all > 5000);  // ten scrolls of 1000 bytes and more
    CHECK(m.snow->accesses == before);
}

TEST_CASE("bw2001 machine: stage 3, the self-test by DIAG on the user port", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.3 s simulated: runs in the release build");
#endif
    std::string text = NetlistText();
    const std::size_t at = text.find("net DIAG ");
    REQUIRE(at != std::string::npos);
    text.replace(at, 9, "net GND ");  // a wire from user port pin 5 to ground
    Running m(1, VariantCircuit(text));
    m.runUntil(1500_ms);
    const auto rows = Bw2001ScreenRows(m);
    REQUIRE(rows.size() >= 12);
    CHECK(rows[0] == "BW-2001 SELF-TEST");
    for (std::size_t r = 2; r <= 9; ++r) {
        INFO(rows[r]);
        CHECK(rows[r].ends_with(" OK"));
    }
    CHECK(rows[11] == "KEY");
}

TEST_CASE("bw2001 machine: Commodore's BASIC 1 and its killer poke", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("seconds simulated: runs in the release build");
#endif
    Running m(1, Bw2001BoardCircuit(), true);
    m.runUntil(1500_ms);
    auto rows = Bw2001ScreenRows(m);
    REQUIRE(rows.size() >= 5);
    CHECK(rows[0] == "*** COMMODORE BASIC ***");
    CHECK(rows[2] == " 7167 BYTES FREE");
    CHECK(rows[4] == "READY.");
    m.runUntil(m.sim->now() + 40_ms);
    CHECK(Bw2001TvRows(m.tv->tv().completedFrame()) == rows);

    // Its editor waits for the retrace before each screen store: printing makes no snow.
    const int before = m.snow->accesses;
    Bw2001Type(m, "PRINT 12\r");
    m.runUntil(m.sim->now() + 300_ms);
    CHECK(m.snow->accesses == before);

    // POKE 59458,62: PB5 an output, its 0 against the video's gate. The wait reads 0 at once and
    // printing snows (scrolling stays blanked); the gate wins the line, so the retrace interrupt
    // and the keyboard go on.
    Bw2001Type(m, "POKE 59458,62\r");
    Bw2001Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r");
    m.runUntil(m.sim->now() + 1500_ms);
    CHECK(m.snow->accesses > before + 40);
    CHECK(m.sim->value(m.loaded->nets.at("VIDEO_ON")) != LogicValue::X);
    Bw2001Type(m, "PRINT 777\r");
    m.runUntil(m.sim->now() + 500_ms);
    rows = Bw2001ScreenRows(m);
    REQUIRE(rows.size() >= 4);  // BASIC 1 leaves a blank row before READY.
    CHECK(rows[rows.size() - 4] == "PRINT 777");
    CHECK(rows[rows.size() - 3] == " 777");
    CHECK(rows[rows.size() - 2].empty());
    CHECK(rows[rows.size() - 1] == "READY.");
}

TEST_CASE("bw2001 machine: stage 3, a P5o session on the Machine", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("tens of seconds simulated: runs in the release build");
#endif
    Running m(2, Bw2001DebugCircuit());
    const auto difference =
        RunBw2001Session(m, std::filesystem::path(BW_ROOT_DIR) / "tests/data/basic/27_cls.txt");
    INFO(difference.value_or(""));
    CHECK_FALSE(difference.has_value());
}
