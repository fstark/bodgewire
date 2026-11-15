// Our BW-2001 firmware on the real Parts (docs/p5p-bw2001-firmware/BW2001_FIRMWARE_DESIGN.md §9,
// §10): tests/data/bw2001_firmware.bwn, P5n's bench with our ROMs, the whole key matrix and DIAG.
// The P5o conformance set runs here only in the release build (hours in debug).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machines/bw2001_harness.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "parts/ram2114.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::filesystem::path NetlistPath() {
    return std::filesystem::path(BW_ROOT_DIR) / "tests/data/bw2001_firmware.bwn";
}

// A matrix place's switch: SW1-SW73 in matrix order, row 0 PB0 first, the unused places skipped.
std::string KeyReference(Bw2001Key key) {
    int n = 0;
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 8; ++column) {
            if (Bw2001KeyLegend({row, column}).empty()) {
                continue;
            }
            ++n;
            if (row == key.row && column == key.column) {
                return "SW" + std::to_string(n);
            }
        }
    }
    FAIL("no key at row " << key.row << " column " << key.column);
    return "";
}

class Bw2001Netlist final : public Bw2001Target {
public:
    explicit Bw2001Netlist(std::uint64_t seed, bool diag = false) {
        std::ifstream in(NetlistPath());
        REQUIRE(in);
        std::stringstream text;
        text << in.rdbuf();
        auto netlist = ParseNetlist(text.str(), "bw2001_firmware.bwn");
        REQUIRE(netlist.has_value());
        ResolveContentsPaths(*netlist, NetlistPath().parent_path());
        auto result = BuildCircuit(*netlist);
        REQUIRE(result.has_value());
        loaded_ = std::move(*result);
        CHECK(loaded_->warnings.empty());
        for (const char* chip : {"UI1", "UI2", "UI3", "UI4", "UI5", "UI6", "UI7", "UI8"}) {
            low_.push_back(dynamic_cast<const Ram2114*>(loaded_->components.at(chip)));
        }
        for (const char* chip : {"UJ1", "UJ2", "UJ3", "UJ4", "UJ5", "UJ6", "UJ7", "UJ8"}) {
            high_.push_back(dynamic_cast<const Ram2114*>(loaded_->components.at(chip)));
        }
        videoLow_ = dynamic_cast<const Ram2114*>(loaded_->components.at("UC3"));
        videoHigh_ = dynamic_cast<const Ram2114*>(loaded_->components.at("UC4"));
        sim_ = std::make_unique<Simulator>(loaded_->circuit);
        if (diag) {
            SwitchOf(*loaded_, "SW74")->setPressed(true);
        }
        sim_->powerOn(seed);
    }

    void run(std::uint64_t us) override {
        sim_->runUntil(sim_->now() + static_cast<Time>(us) * 1_us);
    }
    void setKey(Bw2001Key key, bool held) override {
        Switch* sw = SwitchOf(*loaded_, KeyReference(key));
        REQUIRE(sw != nullptr);
        sw->setPressed(held);
        sim_->wake(sw->id());
    }
    std::uint8_t ram(std::uint16_t address) const override {
        if (address >= 0x2000) {
            return 0xFF;
        }
        const std::size_t chip = address >> 10, word = address & 0x3FF;
        return static_cast<std::uint8_t>(low_[chip]->word(word) | high_[chip]->word(word) << 4);
    }
    std::uint8_t screen(std::size_t offset) const override {
        return static_cast<std::uint8_t>(videoLow_->word(offset) | videoHigh_->word(offset) << 4);
    }
    Time now() const { return sim_->now(); }

private:
    std::unique_ptr<LoadedCircuit> loaded_;
    std::unique_ptr<Simulator> sim_;
    std::vector<const Ram2114*> low_, high_;
    const Ram2114* videoLow_ = nullptr;
    const Ram2114* videoHigh_ = nullptr;
};

}  // namespace

TEST_CASE("bw2001 netlist: power-on to the banner and READY", "[netlist][slow]") {
    Bw2001Netlist machine(1);
    Bw2001Typist typist(machine);
    REQUIRE(typist.waitForLine());
    CHECK(typist.rows() == std::vector<std::string>{"*** BW-2001 TINY BASIC ***", "",
                                                    "7166 BYTES FREE", "", "READY"});
}

TEST_CASE("bw2001 netlist: session 27_cls through the real matrix", "[netlist][slow]") {
    Bw2001Netlist machine(2);
    const auto difference = RunBw2001Session(machine, std::filesystem::path(BW_ROOT_DIR) /
                                                          "tests/data/basic/27_cls.txt");
    INFO(difference.value_or(""));
    CHECK_FALSE(difference.has_value());
}

TEST_CASE("bw2001 netlist: DIAG to the KEY row", "[netlist][slow]") {
    Bw2001Netlist machine(3, true);
    machine.run(1'200'000);
    machine.setKey({4, 0}, true);  // A
    machine.run(20'000);
    const auto rows = Bw2001Typist(machine).rows();
    REQUIRE(rows.size() == 12);
    CHECK(rows[2] == "ZERO PAGE OK");
    CHECK(rows[3] == "VIDEO RAM OK");
    CHECK(rows[4] == "RAM OK");
    for (std::size_t i = 5; i < 10; ++i) {
        CHECK(rows[i].ends_with(" OK"));
    }
    CHECK(rows[11] == "KEY 40 A");
}

TEST_CASE("bw2001 netlist conformance (release)", "[netlist][slow]") {
#ifndef NDEBUG
    SKIP("release only: the whole set takes hours in debug");
#endif
    // Catch enters the case once a section: each session reports its own ratio (simulated
    // time over wall time, while typing).
    for (const std::filesystem::path& path : Bw2001Sessions()) {
        DYNAMIC_SECTION(path.filename().string()) {
            Bw2001Netlist machine(1);
            const auto start = std::chrono::steady_clock::now();
            const auto difference = RunBw2001Session(machine, path);
            const double wall =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const double simulated = static_cast<double>(machine.now()) / static_cast<double>(1_s);
            UNSCOPED_INFO(path.filename().string() << ": " << simulated << " s simulated in "
                                                   << wall << " s, ratio " << simulated / wall);
            INFO(difference.value_or(""));
            CHECK_FALSE(difference.has_value());
        }
    }
    SUCCEED();
}
