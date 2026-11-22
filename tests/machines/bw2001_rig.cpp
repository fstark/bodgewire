#include "bw2001_rig.hpp"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "bw2001_screen.hpp"
#include "parts/matrix_keyboard.hpp"

namespace bw {

using namespace literals;

std::filesystem::path Bw2001NetlistPath() { return test::NetlistPath("bw2001"); }

std::unique_ptr<LoadedCircuit> Bw2001BoardCircuit() {
    auto machine = LoadNetlistMachine(Bw2001NetlistPath());
    REQUIRE(machine.has_value());
    return Bw2001Built(*machine);
}

std::unique_ptr<LoadedCircuit> Bw2001DebugCircuit() {
    auto machine = LoadNetlistMachine(Bw2001NetlistPath(), test::DebugRomDir("bw2001"));
    REQUIRE(machine.has_value());
    return Bw2001Built(*machine);
}

std::unique_ptr<LoadedCircuit> Bw2001Built(const Machine& machine) {
    auto built = machine.build();
    if (!built) {
        for (const Diagnostic& d : built.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(built.has_value());
    return std::move(*built);
}

namespace {

NetId NetOf(const LoadedCircuit& loaded, std::string_view reference, std::string_view pin) {
    const auto net = PinNet(loaded, reference, pin);
    REQUIRE(net.has_value());
    return *net;
}

}  // namespace

SnowWatch::SnowWatch(const LoadedCircuit& loaded)
    : sel8(NetOf(loaded, "UE5", "4Y")), videoOn(NetOf(loaded, "UC6", "3Y")),
      disOn(NetOf(loaded, "UC5", "2Q")), blankTv(NetOf(loaded, "UG8", "CA2")) {}

void SnowWatch::feed(const Simulator& sim) {
    struct Edge {
        Time time;
        int net;
        LogicValue value;
    };
    std::vector<Edge> edges;
    const NetId nets[4]{sel8, videoOn, disOn, blankTv};
    for (int n = 0; n < 4; ++n) {
        for (const Sample& sample : sim.recorder().samples(nets[n])) {
            if (sample.time > fedUntil) {
                edges.push_back({sample.time, n, sample.value});
            }
        }
    }
    std::ranges::stable_sort(edges, {}, &Edge::time);
    const auto seen = [&] {
        return s == LogicValue::One && v == LogicValue::One && d == LogicValue::One &&
               b == LogicValue::One;
    };
    for (const Edge& e : edges) {
        const bool before = seen();
        (e.net == 0 ? s : e.net == 1 ? v : e.net == 2 ? d : b) = e.value;
        const bool after = seen();
        accesses += !before && after && e.net == 0 ? 1 : 0;
        all += e.net == 0 && e.value == LogicValue::One ? 1 : 0;
    }
    fedUntil = sim.now();
}

Running::Running(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit, bool watchSnow)
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
        snow.emplace(*loaded);
        for (const NetId net : {snow->sel8, snow->videoOn, snow->disOn, snow->blankTv}) {
            sim->record(net);
        }
    }
}

void Running::runUntil(Time t) {
    while (sim->now() < t) {
        sim->runUntil(std::min(t, sim->now() + 1_ms));
        tv->update();
        if (snow) {
            snow->feed(*sim);
        }
        sim->trimRecordings();
    }
}

void Running::run(std::uint64_t us) { runUntil(sim->now() + static_cast<Time>(us) * 1_us); }

void Running::setKey(Bw2001Key key, bool held) {
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*loaded, "J5");
    REQUIRE(keyboard != nullptr);
    const auto index = Bw2001KeyAt(key.row, key.column);
    REQUIRE(index.has_value());
    keyboard->setHeld(*index, held);
    sim->wake(keyboard->id());
}

std::uint8_t Running::ram(std::uint16_t address) const {
    const std::size_t chip = address >> 10, word = address & 0x3FF;
    if (address >= 0x2000 || low[chip] == nullptr) {
        return 0xFF;
    }
    return static_cast<std::uint8_t>(low[chip]->word(word) | high[chip]->word(word) << 4);
}

std::uint8_t Running::screen(std::size_t offset) const {
    return static_cast<std::uint8_t>(videoLow->word(offset) | videoHigh->word(offset) << 4);
}

std::vector<std::string> Bw2001ScreenRows(const Running& m) {
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

void Bw2001Type(Running& m, std::string_view text) {
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*m.loaded, "J5");
    REQUIRE(keyboard != nullptr);
    for (const char c : text) {
        REQUIRE(keyboard->type(static_cast<unsigned char>(c)));
    }
    m.sim->wake(keyboard->id());
    m.runUntil(m.sim->now() + static_cast<Time>(text.size()) * (kMatrixHold + kMatrixGap) + 20_ms);
}

bool Bw2001WaitForRows(Running& m, const std::vector<std::string>& rows, Time limit) {
    const Time until = m.sim->now() + limit;
    while (Bw2001ScreenRows(m) != rows) {
        if (m.sim->now() >= until) {
            return false;
        }
        m.runUntil(m.sim->now() + 1_ms);
    }
    return true;
}

}  // namespace bw
