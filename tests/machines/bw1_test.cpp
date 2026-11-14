// Machine 120 BW-1 (docs/machines/120-bw1/BW1_DESIGN.md, Behaviour checked): the monitor and the
// self-test of P5k on the real Parts, keys pressed as a player would.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "bw1_model.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kHold = 20_ms;  // a key held, then released as long: two scans of 3.5 ms each
constexpr Time kPoll = 20_us;  // display(): a digit is selected about 550 us a scan

std::filesystem::path NetlistPath() { return std::filesystem::path(BW_NETLIST_DIR) / "bw1.bwn"; }

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(NetlistPath());
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The netlist with U24 output 3 joined to PA0: the self-test strap.
std::unique_ptr<LoadedCircuit> StrappedCircuit() {
    std::ifstream in(NetlistPath());
    REQUIRE(in);
    std::stringstream text;
    text << in.rdbuf();
    std::string source = text.str();
    const std::string strap = "net STRAP    U24.Y3_N";
    const std::size_t at = source.find(strap);
    REQUIRE(at != std::string::npos);
    source.replace(at, strap.size(), "net PA0      U24.Y3_N");
    auto netlist = ParseNetlist(source, "bw1.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, NetlistPath().parent_path());
    auto result = BuildCircuit(*netlist);
    REQUIRE(result.has_value());
    CHECK((*result)->warnings.empty());
    return std::move(*result);
}

// The switch reference of a key name: a hex digit, AD, DA, +, GO, PC, ST or RS.
std::string KeyReference(std::string_view name) {
    if (name.size() == 1 && std::isxdigit(static_cast<unsigned char>(name[0]))) {
        return "SW" + std::to_string(std::stoi(std::string(name), nullptr, 16) + 1);
    }
    constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kNamed{{
        {"AD", "SW17"},
        {"DA", "SW18"},
        {"+", "SW19"},
        {"GO", "SW20"},
        {"PC", "SW21"},
        {"ST", "SW22"},
        {"RS", "SW23"},
    }};
    for (const auto& [key, reference] : kNamed) {
        if (key == name) {
            return std::string(reference);
        }
    }
    FAIL("unknown key " << name);
    return {};
}

// A powered BW-1 and its keypad.
class Bw1 {
public:
    Bw1(std::unique_ptr<LoadedCircuit> loaded, std::uint64_t seed)
        : loaded_(std::move(loaded)), sim_(loaded_->circuit) {
        for (int k = 1; k <= 24; ++k) {
            Switch* sw = SwitchOf(*loaded_, "SW" + std::to_string(k));
            REQUIRE(sw != nullptr);
            sw->setPressed(false);
        }
        for (int d = 0; d < 6; ++d) {
            digits_[d] = loaded_->nets.at("DIG" + std::to_string(d + 1));
        }
        for (int s = 0; s < 7; ++s) {
            segments_[s] = loaded_->nets.at(std::string("SEG_") + static_cast<char>('A' + s));
        }
        sim_.powerOn(seed);
    }

    Time now() const { return sim_.now(); }
    void run(Time time) { sim_.runUntil(sim_.now() + time); }
    void runUntil(Time time) { sim_.runUntil(time); }

    void set(std::string_view reference, bool pressed) {
        Switch* sw = SwitchOf(*loaded_, reference);
        sw->setPressed(pressed);
        sim_.wake(sw->id());
    }
    void hold(std::string_view key) { set(KeyReference(key), true); }
    void release(std::string_view key) { set(KeyReference(key), false); }
    void press(std::string_view key) {
        hold(key);
        run(kHold);
        release(key);
        run(kHold);
    }
    // "AD 0300 DA A9 +": key names, and hex digits one a key; blanks separate.
    void type(std::string_view keys) {
        std::size_t i = 0;
        while (i < keys.size()) {
            if (keys[i] == ' ') {
                ++i;
                continue;
            }
            std::size_t end = keys.find(' ', i);
            end = end == std::string_view::npos ? keys.size() : end;
            const std::string_view word = keys.substr(i, end - i);
            const bool named = word == "AD" || word == "DA" || word == "GO" || word == "PC" ||
                               word == "ST" || word == "RS";
            if (named) {
                press(word);
            } else {
                for (const char c : word) {
                    press(std::string_view(&c, 1));
                }
            }
            i = end;
        }
    }
    void setSst(bool on) { set("SW24", on); }

    // The six digits over the next `window`, as Bw1Model::display reads them.
    std::string display(Time window = 20_ms) {
        std::array<int, 6> digitOn{};
        std::array<std::array<int, 7>, 6> segmentOn{};
        int polls = 0;
        const Time end = sim_.now() + window;
        while (sim_.now() < end) {
            run(kPoll);
            ++polls;
            int selected = -1;
            for (int d = 0; d < 6; ++d) {
                if (sim_.value(digits_[d]) == Zero && !sim_.isFloating(digits_[d])) {
                    selected = selected == -1 ? d : -2;
                }
            }
            if (selected < 0) {
                continue;
            }
            ++digitOn[selected];
            for (int s = 0; s < 7; ++s) {
                if (sim_.value(segments_[s]) == One && !sim_.isFloating(segments_[s])) {
                    ++segmentOn[selected][s];
                }
            }
        }
        std::string shown;
        for (int d = 0; d < 6; ++d) {
            if (digitOn[d] * 20 < polls) {
                shown += ' ';
                continue;
            }
            std::uint8_t lit = 0;
            for (int s = 0; s < 7; ++s) {
                if (segmentOn[d][s] * 2 >= digitOn[d]) {
                    lit = static_cast<std::uint8_t>(lit | (1u << s));
                }
            }
            shown += Bw1DigitChar(lit);
        }
        return shown;
    }

private:
    std::unique_ptr<LoadedCircuit> loaded_;
    Simulator sim_;
    std::array<NetId, 6> digits_{};
    std::array<NetId, 7> segments_{};
};

bool IsHex(std::string_view text) {
    return !text.empty() && std::ranges::all_of(text, [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || (c >= 'A' && c <= 'F');
    });
}

// "AAAA" and two hex digits: the address shown with the byte there.
void CheckShows(const std::string& shown, std::string_view address) {
    INFO(shown);
    CHECK(shown.substr(0, 4) == address);
    CHECK(IsHex(shown.substr(4)));
}

// After power-on: the reset is over at 20 ms, the monitor shows 0200 by about 25 ms.
Bw1 Started(std::unique_ptr<LoadedCircuit> loaded, std::uint64_t seed) {
    Bw1 bw1(std::move(loaded), seed);
    bw1.runUntil(40_ms);
    return bw1;
}

}  // namespace

TEST_CASE("bw1: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    CHECK(loaded->warnings.empty());
}

TEST_CASE("bw1: reset shows 0200", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    Bw1 bw1 = Started(DesignCircuit(), seed);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: keys set the address and write memory", "[machines][slow]") {
    Bw1 bw1 = Started(DesignCircuit(), 1);
    bw1.type("AD 0300 DA A9 +");
    CheckShows(bw1.display(), "0301");
    bw1.type("AD 0300");
    CHECK(bw1.display() == "0300A9");
}

TEST_CASE("bw1: a program typed in runs", "[machines][slow]") {
    Bw1 bw1 = Started(DesignCircuit(), 1);
    bw1.type("DA A9 + 42 + 85 + 10 + 00 AD 0200 GO");  // LDA #$42, STA $10, BRK
    CheckShows(bw1.display(), "0206");
    bw1.type("AD 0010");
    CHECK(bw1.display() == "001042");
}

TEST_CASE("bw1: ST stops a loop", "[machines][slow]") {
    Bw1 bw1 = Started(DesignCircuit(), 1);
    bw1.type("DA 4C + 00 + 02 AD 0200 GO");  // JMP $0200
    bw1.run(50_ms);
    bw1.press("ST");
    CHECK(bw1.display() == "02004C");
}

TEST_CASE("bw1: SST steps one instruction", "[machines][slow]") {
    Bw1 bw1 = Started(DesignCircuit(), 1);
    bw1.type("DA A9 + 11 + A9 + 22 AD 0200");  // LDA #$11, LDA #$22
    bw1.setSst(true);
    bw1.type("GO");
    CHECK(bw1.display() == "0202A9");
    bw1.type("GO");
    CheckShows(bw1.display(), "0204");  // past the program: RAM's power-on byte
}

TEST_CASE("bw1: RS returns to 0200", "[machines]") {
    Bw1 bw1 = Started(DesignCircuit(), 1);
    bw1.type("AD 1234");
    CheckShows(bw1.display(), "1234");
    bw1.press("RS");
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: the strap runs the self-test", "[machines][slow]") {
    Bw1 bw1(StrappedCircuit(), 1);
    bw1.runUntil(22_ms);
    CHECK(bw1.display(5_ms) == "1     ");
    bw1.runUntil(300_ms);
    CHECK(bw1.display() == "888888");
    bw1.runUntil(3200_ms);
    CHECK(bw1.display() == "PA55  ");
    bw1.hold("GO");
    bw1.run(10_ms);
    CHECK(bw1.display() == "PA5513");
}
