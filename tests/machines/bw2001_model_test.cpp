// The BW-2001 model and harness (docs/p5p-bw2001-firmware/BW2001_FIRMWARE_DESIGN.md §7-§9), and
// the firmware's ROMs read as files.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "asm/assembler.hpp"
#include "bw2001_harness.hpp"
#include "bw2001_model.hpp"

using namespace bw;

namespace {

// A program in UH7 alone: PIA1 set as the kernel does; in the CB1 interrupt, row 3 read into $0300
// and the retraces counted at $0301. Port B is read only there: a read clears the retrace flag.
constexpr std::string_view kProbe = R"(
        .rom $F800, $800
        .org $F800
reset:  sei
        cld
        ldx #$FF
        txs
        lda #0
        sta $0301
        sta $E811
        lda #$0F
        sta $E810
        lda #$04
        sta $E811
        lda #0
        sta $E813
        sta $E812
        lda #$05
        sta $E813
        cli
loop:   jmp loop
irq:    pha
        lda $E813
        bpl @done
        lda #3
        sta $E810
        lda $E812
        sta $0300
        inc $0301
@done:  pla
        rti
        .org $FFFA
        .word irq, reset, irq
)";

std::uint16_t IncValue(const std::string& name) {
    std::ifstream in(test::RomPath("bw2001", "bw2001.inc"));
    REQUIRE(in);
    const std::regex line("^" + name + R"(\s*=\s*\$([0-9A-Fa-f]+))");
    for (std::string text; std::getline(in, text);) {
        std::smatch m;
        if (std::regex_search(text, m, line)) {
            return static_cast<std::uint16_t>(std::stoul(m[1].str(), nullptr, 16));
        }
    }
    FAIL("bw2001.inc has no " << name);
    return 0;
}

}  // namespace

TEST_CASE("bw2001 model: PIA1 reads the matrix and the retrace interrupts", "[bw2001]") {
    const auto rom = Assemble(kProbe, "probe.asm");
    REQUIRE(rom.has_value());
    Bw2001Model model({.seed = 1, .roms = {{0xF800, rom->words}}});
    model.run(20'000);
    CHECK(model.peek(0x0300) == 0xFF);
    model.setKey({3, 4}, true);  // P
    model.setKey({4, 4}, true);  // L: another row, not seen
    model.run(20'000);
    CHECK(model.peek(0x0300) == 0xEF);
    model.setKey({3, 4}, false);
    model.run(20'000);
    CHECK(model.peek(0x0300) == 0xFF);
    const std::uint8_t before = model.peek(0x0301);
    model.run(100'000);
    CHECK(model.peek(0x0301) - before == 6);
    CHECK_FALSE(model.touchedUnmodelled());
    model.setDiag(true);
    CHECK((model.peek(0xE810) & 0x80) == 0);
    model.setDiag(false);
    CHECK((model.peek(0xE810) & 0x80) != 0);
}

TEST_CASE("bw2001 model: stuck bits, dead RAM, the RAM's top", "[bw2001]") {
    const auto rom = Assemble(kProbe, "probe.asm");
    REQUIRE(rom.has_value());
    Bw2001Model model({.seed = 2, .ramTop = 0x1000, .roms = {{0xF800, rom->words}}});
    model.poke(0x0234, 0x00);
    model.stick(0x0234, 3, true);
    CHECK(model.peek(0x0234) == 0x08);
    model.poke(0x8010, 0xFF);
    model.stick(0x8010, 7, false);
    CHECK(model.screen(0x10) == 0xFF);  // screen() is the cells as held
    CHECK(model.peek(0x8010) == 0x7F);
    model.deadRam();
    CHECK(model.peek(0x0000) == 0xFF);
}

TEST_CASE("bw2001 harness: the key map and the shared file agree", "[bw2001]") {
    for (int c = 0x20; c <= 0x5F; ++c) {
        INFO("character " << static_cast<char>(c));
        CHECK(Bw2001KeyFor(static_cast<char>(c)).has_value());
    }
    CHECK(Bw2001KeyFor('a') == std::nullopt);
    CHECK(Bw2001KeyFor('1') == Bw2001Key{6, 6});
    CHECK(Bw2001KeyFor(' ') == Bw2001Key{9, 2});
    CHECK(Bw2001KeyLegend(kBw2001Return) == "RETURN");
    CHECK(Bw2001KeyLegend(kBw2001Stop) == "STOP");
    int used = 0;
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 8; ++column) {
            used += Bw2001KeyLegend({row, column}).empty() ? 0 : 1;
        }
    }
    CHECK(used == 73);
    CHECK(IncValue("WAITING") == kBw2001Waiting);
    CHECK(IncValue("LINES") == kBw2001Lines);
    CHECK(IncValue("USRRAM") == kBw2001SessionRam);
    CHECK(Bw2001Sessions().size() >= 31);
}

namespace {

// A fake target whose screen shows `rows` once a line has been typed, and whose editor waits
// whenever no key is held: enough to drive RunBw2001Session's parsing and comparing.
class FakeTarget final : public Bw2001Target {
public:
    explicit FakeTarget(std::vector<std::string> rows) : rows_(std::move(rows)) {}
    void run(std::uint64_t) override {}
    void setKey(Bw2001Key key, bool held) override {
        if (key == kBw2001Return && held) {
            ++lines_;
        }
    }
    std::uint8_t ram(std::uint16_t address) const override {
        return address == kBw2001Waiting ? 1 : address == kBw2001Lines ? lines_ : 0;
    }
    std::uint8_t screen(std::size_t offset) const override {
        const std::size_t row = offset / 40, column = offset % 40;
        if (row >= rows_.size() || column >= rows_[row].size()) {
            return 0x20;
        }
        const char c = rows_[row][column];
        return static_cast<std::uint8_t>(c >= 0x40 ? c - 0x40 : c);
    }

private:
    std::vector<std::string> rows_;
    std::uint8_t lines_ = 0;
};

std::filesystem::path Session(std::string_view text) {
    const auto path = std::filesystem::temp_directory_path() / "bw2001_harness_session.txt";
    std::ofstream(path) << text;
    return path;
}

}  // namespace

TEST_CASE("bw2001 harness: a session's full view is compared", "[bw2001]") {
    const auto path = Session("~ a comment\nREADY\n|PRINT {RAM}\n1008\nREADY\n");
    FakeTarget good({"READY", "PRINT 1008", "1008", "READY"});
    CHECK(RunBw2001Session(good, path) == std::nullopt);
    FakeTarget bad({"READY", "PRINT 1008", "1009", "READY"});
    const auto error = RunBw2001Session(bad, path);
    REQUIRE(error.has_value());
    CHECK(error->starts_with("row 2: expected '1008', got '1009'"));
    FakeTarget longer({"READY", "PRINT 1008", "1008", "READY", "X"});
    CHECK(RunBw2001Session(longer, path)->starts_with("row 4: expected '(nothing)', got 'X'"));
    const auto cleared = Session("|10 PRINT 1\n~clear\nREADY\n");
    FakeTarget afterClear({"READY"});
    CHECK(RunBw2001Session(afterClear, cleared) == std::nullopt);
    CHECK(RunBw2001Session(afterClear, Session("~bogus\n")) == "unknown directive '~bogus'");
    CHECK(RunBw2001Session(afterClear, Session("READY\n~break\n")) ==
          "~break before any typed line");
}
