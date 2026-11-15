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

#include "../netlist/external_rom.hpp"
#include "bw2001_harness.hpp"
#include "bw2001_screen.hpp"
#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "parts/matrix_keyboard.hpp"
#include "parts/ram2114.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::filesystem::path NetlistPath() { return std::filesystem::path(BW_NETLIST_DIR) / "bw2001.bwn"; }

std::string NetlistText() {
    std::ifstream in(NetlistPath());
    REQUIRE(in);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// The netlist edited as text (a test's variant: DIAG grounded, BASIC 1 in the sockets), built.
std::unique_ptr<LoadedCircuit> VariantCircuit(const std::string& text) {
    auto netlist = ParseNetlist(text, "bw2001.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, NetlistPath().parent_path());
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

// Commodore's BASIC 1 in the seven sockets, the empty UH2 and UH6 fitted as UH1 and UH5 are
// (their selects on SELD_N); nullptr without the user's ROMs.
std::unique_ptr<LoadedCircuit> Basic1Circuit() {
    for (const PetRom& rom : kPetBasic1) {
        if (!ExternalRom(rom.file)) {
            return nullptr;
        }
    }
    std::istringstream in(NetlistText());
    std::string out, line;
    while (std::getline(in, line)) {
        std::string extra;
        std::istringstream words(line.substr(0, line.find('#')));
        std::string word;
        words >> word;
        if (word == "net") {
            words >> word;  // the Net's name
            while (words >> word) {
                for (const auto& [from, to] :
                     {std::pair{"UH1.", "UH2."}, std::pair{"UH5.", "UH6."}}) {
                    if (word.starts_with(from) && word != std::string(from) + "CS1") {
                        extra += " " + std::string(to) + word.substr(4);
                    }
                }
            }
        }
        out += line.substr(0, line.find('#')) + extra + "\n";
    }
    out += "part UH2 2316B select=LLH contents=x.bin\npart UH6 2316B select=LHH contents=x.bin\n"
           "net SELD_N UH2.CS1 UH6.CS1\n";
    for (const PetRom& rom : kPetBasic1) {
        const std::string socket(rom.socket);
        const std::size_t at = out.find("part " + socket + " ");
        REQUIRE(at != std::string::npos);
        const std::size_t contents = out.find("contents=", at);
        const std::size_t end = out.find_first_of(" \n", contents);
        out.replace(contents, end - contents, "contents=" + ExternalRom(rom.file)->string());
    }
    // offset= belonged to our ROM files.
    for (std::size_t at; (at = out.find(" offset=")) != std::string::npos;) {
        out.erase(at, out.find_first_of(" \n", at + 1) - at);
    }
    return VariantCircuit(out);
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(NetlistPath());
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(NetlistPath());
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// Screen accesses while the beam draws the text (SEL8, VIDEO_ON and DIS_ON all high): the PET's
// snow. Fed each slice's samples before they are trimmed.
struct SnowWatch {
    NetId sel8, videoOn, disOn;
    LogicValue s = LogicValue::Zero, v = LogicValue::Zero, d = LogicValue::Zero;
    Time fedUntil = -1;
    int accesses = 0;  // rises into all three high
    int all = 0;       // SEL8's rises: every screen access

    void feed(const Simulator& sim) {
        struct Edge {
            Time time;
            int net;
            LogicValue value;
        };
        std::vector<Edge> edges;
        const NetId nets[3]{sel8, videoOn, disOn};
        for (int n = 0; n < 3; ++n) {
            for (const Sample& sample : sim.recorder().samples(nets[n])) {
                if (sample.time > fedUntil) {
                    edges.push_back({sample.time, n, sample.value});
                }
            }
        }
        std::ranges::stable_sort(edges, {}, &Edge::time);
        for (const Edge& e : edges) {
            const bool before =
                s == LogicValue::One && v == LogicValue::One && d == LogicValue::One;
            (e.net == 0 ? s : e.net == 1 ? v : d) = e.value;
            const bool after = s == LogicValue::One && v == LogicValue::One && d == LogicValue::One;
            accesses += !before && after && e.net == 0 ? 1 : 0;
            all += e.net == 0 && e.value == LogicValue::One ? 1 : 0;
        }
        fedUntil = sim.now();
    }
};

// The Machine running, its TV on the MONITOR connector; a harness target (P5p's typist and
// sessions run on it as on the model).
struct Running final : Bw2001Target {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;
    std::unique_ptr<TvInput> tv;
    std::optional<SnowWatch> snow;
    const Ram2114* videoLow = nullptr;
    const Ram2114* videoHigh = nullptr;
    std::vector<const Ram2114*> low, high;

    explicit Running(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit = BoardCircuit(),
                     bool watchSnow = false)
        : loaded(std::move(circuit)) {
        sim = std::make_unique<Simulator>(loaded->circuit);
        sim->powerOn(seed);
        tv = std::make_unique<TvInput>(*sim,
                                       SeparateDrives{*ConnectorNet(*loaded, "MONITOR", "VIDEO_N"),
                                                      *ConnectorNet(*loaded, "MONITOR", "HDRIVE"),
                                                      *ConnectorNet(*loaded, "MONITOR", "VDRIVE")});
        videoLow = dynamic_cast<const Ram2114*>(loaded->components.at("UC3"));
        videoHigh = dynamic_cast<const Ram2114*>(loaded->components.at("UC4"));
        for (int chip = 8; chip >= 1; --chip) {  // UI8/UJ8 hold $0000 (SHEET2.md)
            const auto find = [&](std::string ref) -> const Ram2114* {
                const auto it = loaded->components.find(ref);
                return it == loaded->components.end() ? nullptr
                                                      : dynamic_cast<const Ram2114*>(it->second);
            };
            low.push_back(find("UI" + std::to_string(chip)));
            high.push_back(find("UJ" + std::to_string(chip)));
        }
        if (watchSnow) {
            snow = SnowWatch{loaded->nets.at("SEL8"), loaded->nets.at("VIDEO_ON"),
                             loaded->nets.at("DIS_ON")};
            for (const NetId net : {snow->sel8, snow->videoOn, snow->disOn}) {
                sim->record(net);
            }
        }
    }
    void runUntil(Time t) {
        while (sim->now() < t) {
            sim->runUntil(std::min(t, sim->now() + 1_ms));
            tv->update();
            if (snow) {
                snow->feed(*sim);
            }
            sim->trimRecordings();
        }
    }
    void run(std::uint64_t us) override { runUntil(sim->now() + static_cast<Time>(us) * 1_us); }
    void setKey(Bw2001Key key, bool held) override {
        MatrixKeyboard* keyboard = MatrixKeyboardOf(*loaded, "J5");
        REQUIRE(keyboard != nullptr);
        const auto index = Bw2001KeyAt(key.row, key.column);
        REQUIRE(index.has_value());
        keyboard->setHeld(*index, held);
        sim->wake(keyboard->id());
    }
    std::uint8_t ram(std::uint16_t address) const override {
        const std::size_t chip = address >> 10, word = address & 0x3FF;
        if (address >= 0x2000 || low[chip] == nullptr) {
            return 0xFF;
        }
        return static_cast<std::uint8_t>(low[chip]->word(word) | high[chip]->word(word) << 4);
    }
    std::uint8_t screen(std::size_t offset) const override {
        return static_cast<std::uint8_t>(videoLow->word(offset) | videoHigh->word(offset) << 4);
    }
};

const std::vector<std::string> kBanner{"*** BW-2001 TINY BASIC ***", "", "7166 BYTES FREE", "",
                                       "READY"};

}  // namespace

TEST_CASE("bw2001 machine: the netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("bw2001 machine: stage 1, the TV locks on 260 lines at 60 Hz", "[machines][slow]") {
    Running m(1);
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
    Running m(seed);
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

TEST_CASE("bw2001 machine: stage 2, our firmware's banner on the TV, no snow", "[machines][slow]") {
    Running m(1, BoardCircuit(), true);
    const NetId led = m.loaded->nets.at("DIAG_LED_N");
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine(3'000'000));
    UNSCOPED_INFO("READY at " << m.sim->now() / 1_ms << " ms");
    CHECK(typist.rows() == kBanner);
    m.runUntil(m.sim->now() + 40_ms);  // two whole frames after the last print
    CHECK(Bw2001TvRows(m.tv->tv().completedFrame()) == kBanner);
    CHECK(Bw2001BorderDots(m.tv->tv().completedFrame()) == 0);
    CHECK(m.snow->all > 1000);     // the clear (1000 bytes) and the banner: the watch sees them
    CHECK(m.snow->accesses == 0);  // the editor touches the screen only in the retrace
    CHECK(m.sim->value(led) == LogicValue::One);  // DS1 dark: no software selects row 11
}

namespace {

// The video RAM as text rows, as typist.rows() (P5p's screen codes; BASIC 1 uses the same).
std::vector<std::string> ScreenRows(const Running& m) {
    std::vector<std::string> rows;
    for (int row = 0; row < kBw2001Rows; ++row) {
        std::string text;
        for (int column = 0; column < kBw2001Columns; ++column) {
            text.push_back(Bw2001ScreenChar(m.screen(static_cast<std::size_t>(row * 40 + column))));
        }
        text.erase(text.find_last_not_of(' ') + 1);
        rows.push_back(text);
    }
    while (!rows.empty() && rows.back().empty()) {
        rows.pop_back();
    }
    return rows;
}

// Types `text` on J5 (40 ms down, 40 ms up a character) and runs until it is all typed.
void Type(Running& m, std::string_view text) {
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*m.loaded, "J5");
    REQUIRE(keyboard != nullptr);
    for (const char c : text) {
        REQUIRE(keyboard->type(static_cast<unsigned char>(c)));
    }
    m.sim->wake(keyboard->id());
    m.runUntil(m.sim->now() + static_cast<Time>(text.size()) * (kMatrixHold + kMatrixGap) + 20_ms);
}

}  // namespace

TEST_CASE("bw2001 machine: stage 3, typing through the keyboard connector", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.3 s simulated: minutes in debug; runs in the release build");
#endif
    Running m(2);
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine());
    Type(m, "PRINT 2+2\r");
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
    Running m(3, BoardCircuit(), true);
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine());
    Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r");
    REQUIRE(typist.waitForLine(10'000'000));
    const auto rows = typist.rows();
    REQUIRE(rows.size() == 24);  // scrolled: the cursor's row is the 25th
    CHECK(rows[21] == "29");
    CHECK(rows[22] == "30");
    CHECK(rows[23] == "READY");
    CHECK(m.snow->all > 5000);  // ten scrolls of 1000 bytes and more
    CHECK(m.snow->accesses == 0);
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
    const auto rows = ScreenRows(m);
    REQUIRE(rows.size() >= 12);
    CHECK(rows[0] == "BW-2001 SELF-TEST");
    for (std::size_t r = 2; r <= 9; ++r) {
        INFO(rows[r]);
        CHECK(rows[r].ends_with(" OK"));
    }
    CHECK(rows[11] == "KEY");
}

TEST_CASE("bw2001 machine: the real BASIC 1 and its killer poke", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("seconds simulated: runs in the release build");
#endif
    auto circuit = Basic1Circuit();
    if (!circuit) {
        SKIP(kNoPetRoms);
    }
    Running m(1, std::move(circuit), true);
    m.runUntil(1500_ms);
    auto rows = ScreenRows(m);
    REQUIRE(rows.size() >= 5);
    CHECK(rows[0] == "*** COMMODORE BASIC ***");
    CHECK(rows[2] == " 7167 BYTES FREE");
    CHECK(rows[4] == "READY.");
    m.runUntil(m.sim->now() + 40_ms);
    CHECK(Bw2001TvRows(m.tv->tv().completedFrame()) == rows);

    // Its editor waits for the retrace before each screen store: printing makes no snow.
    const int before = m.snow->accesses;
    Type(m, "PRINT 12\r");
    m.runUntil(m.sim->now() + 300_ms);
    CHECK(m.snow->accesses == before);

    // POKE 59458,62: PB5 an output, its 0 against the video's gate. The wait reads 0 at once and
    // printing snows; the gate wins the line, so the retrace interrupt and the keyboard go on.
    Type(m, "POKE 59458,62\r");
    Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r");
    m.runUntil(m.sim->now() + 1500_ms);
    CHECK(m.snow->accesses > before + 100);
    CHECK(m.sim->value(m.loaded->nets.at("VIDEO_ON")) != LogicValue::X);
    Type(m, "PRINT 777\r");
    m.runUntil(m.sim->now() + 500_ms);
    rows = ScreenRows(m);
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
    Running m(2);
    const auto difference =
        RunBw2001Session(m, std::filesystem::path(BW_ROOT_DIR) / "tests/data/basic/27_cls.txt");
    INFO(difference.value_or(""));
    CHECK_FALSE(difference.has_value());
}
