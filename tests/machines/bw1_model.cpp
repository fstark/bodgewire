#include "bw1_model.hpp"

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <random>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "parts/rom_contents.hpp"

namespace bw {

namespace {

constexpr std::size_t kMainRam = 1024, kRiotRam = 64;

int Block(std::uint16_t address) { return (address >> 10) & 7; }  // U4: A10-A12, A13-A15 free

std::vector<std::uint8_t> LoadRom(std::string_view name) {
    const auto words = ReadRomContents(test::RomPath("bw1", name), 1024, 8);
    if (!words) {
        FAIL(words.error());
    }
    return *words;
}

}  // namespace

char Bw1DigitChar(std::uint8_t segments) {
    // Segment patterns (PA0 = a ... PA6 = g) and what they show.
    struct Glyph {
        std::uint8_t segments;
        char shown;
    };
    constexpr std::array<Glyph, 19> kGlyphs{{
        {0x3F, '0'}, {0x06, '1'}, {0x5B, '2'}, {0x4F, '3'}, {0x66, '4'}, {0x6D, '5'}, {0x7D, '6'},
        {0x07, '7'}, {0x7F, '8'}, {0x6F, '9'}, {0x77, 'A'}, {0x7C, 'B'}, {0x39, 'C'}, {0x5E, 'D'},
        {0x79, 'E'}, {0x71, 'F'}, {0x73, 'P'}, {0x40, '-'}, {0x00, ' '},
    }};
    for (const Glyph& g : kGlyphs) {
        if (g.segments == segments) {
            return g.shown;
        }
    }
    return '?';
}

Bw1Model::Bw1Model(std::uint64_t seed)
    : rom002_(LoadRom("bw1_002.rom")), rom003_(LoadRom("bw1_003.rom")),
      cells_(kMainRam + 2 * kRiotRam), stuckMask_(cells_.size()), stuckValue_(cells_.size()) {
    std::mt19937_64 random(seed);
    for (auto& byte : cells_) {
        byte = static_cast<std::uint8_t>(random());
    }
    for (auto& riot : riot_) {
        for (auto& reg : riot.reg) {
            reg = static_cast<std::uint8_t>(random());
        }
    }
    core_.start({.pc = static_cast<std::uint16_t>(random()),
                 .a = static_cast<std::uint8_t>(random()),
                 .x = static_cast<std::uint8_t>(random()),
                 .y = static_cast<std::uint8_t>(random()),
                 .s = static_cast<std::uint8_t>(random()),
                 .p = static_cast<std::uint8_t>(random())});
    core_.setIrq(false);
    displayChanged();
    reset();
}

void Bw1Model::reset() {
    core_.setReset(true);
    for (auto& riot : riot_) {
        riot.reg = {};
    }
    displayChanged();
    run(8);
    core_.setReset(false);
}

void Bw1Model::run(std::uint64_t us) {
    for (std::uint64_t i = 0; i < us; ++i) {
        cycle();
    }
}

void Bw1Model::cycle() {
    const Mos6502Bus bus = core_.bus();
    // KIM Fig. 3.3: ST through the 556, SST joining NMI to NAND(SYNC, K7).
    core_.setNmi(stHeld_ || (sst_ && bus.sync && Block(bus.address) != 7));
    if (bus.read) {
        core_.step(read(bus.address));
    } else {
        write(bus.address, bus.data);
        core_.step(0);
    }
    ++cycle_;
}

void Bw1Model::hold(int key) {
    if (key < 0 || key > kKeyPc) {
        throw std::invalid_argument("Bw1Model::hold: key code 0x00-0x14");
    }
    key_ = key;
    displayChanged();
}

void Bw1Model::release() {
    key_.reset();
    displayChanged();
}

void Bw1Model::press(int key, std::uint64_t holdUs, std::uint64_t gapUs) {
    hold(key);
    run(holdUs);
    release();
    run(gapUs);
}

void Bw1Model::type(std::string_view keys) {
    std::size_t i = 0;
    while (i < keys.size()) {
        if (keys[i] == ' ') {
            ++i;
            continue;
        }
        const std::string_view rest = keys.substr(i);
        if (rest.starts_with("AD") || rest.starts_with("DA") || rest.starts_with("GO") ||
            rest.starts_with("PC")) {
            const std::string_view name = rest.substr(0, 2);
            press(name == "AD" ? kKeyAd : name == "DA" ? kKeyDa : name == "GO" ? kKeyGo : kKeyPc);
            i += 2;
        } else if (keys[i] == '+') {
            press(kKeyPlus);
            ++i;
        } else if (std::isxdigit(static_cast<unsigned char>(keys[i]))) {
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(keys[i])));
            press(c <= '9' ? c - '0' : c - 'A' + 10);
            ++i;
        } else {
            throw std::invalid_argument("Bw1Model::type: unknown key");
        }
    }
}

void Bw1Model::stop(std::uint64_t holdUs) {
    stHeld_ = true;
    run(holdUs);
    stHeld_ = false;
}

void Bw1Model::setStrap(bool closed) {
    strap_ = closed;
    displayChanged();
}

std::string Bw1Model::display(std::uint64_t us) {
    displayChanged();
    digitOn_ = {};
    segmentOn_ = {};
    run(us);
    displayChanged();
    std::string shown;
    for (std::size_t d = 0; d < 6; ++d) {
        if (digitOn_[d] * 20 < us) {
            shown += ' ';
            continue;
        }
        std::uint8_t segments = 0;
        for (std::size_t s = 0; s < 7; ++s) {
            if (segmentOn_[d][s] * 2 >= digitOn_[d]) {
                segments = static_cast<std::uint8_t>(segments | (1u << s));
            }
        }
        shown += Bw1DigitChar(segments);
    }
    return shown;
}

std::optional<std::size_t> Bw1Model::cell(std::uint16_t address) const {
    const int block = Block(address);
    if (block == 0) {
        return address & 0x3FF;
    }
    if (block == 5) {
        const unsigned a96 = (address >> 6) & 0xF;
        if (a96 == 0xE) {
            return kMainRam + (address & 0x3F);  // 003 RAM, $1780
        }
        if (a96 == 0xF) {
            return kMainRam + kRiotRam + (address & 0x3F);  // 002 RAM, $17C0
        }
    }
    return std::nullopt;
}

std::uint8_t Bw1Model::paPins() const {
    const Riot& r = riot_[0];
    // An output drives its data bit (1 through the pull-up); an input is pulled up.
    auto pins = static_cast<std::uint8_t>(r.reg[0] | ~r.reg[1]);
    const int row = u24();
    if (key_ && *key_ / 7 == row) {
        pins = static_cast<std::uint8_t>(pins & ~(1u << (6 - *key_ % 7)));
    }
    if (strap_ && row == 3) {
        pins = static_cast<std::uint8_t>(pins & ~1u);
    }
    return pins;
}

std::uint8_t Bw1Model::pbPins() const {
    const Riot& r = riot_[0];
    return static_cast<std::uint8_t>(r.reg[2] | ~r.reg[3]);
}

std::uint8_t Bw1Model::peek(std::uint16_t address) const {
    if (const auto c = cell(address)) {
        return cells_[*c];
    }
    if (Block(address) == 6 || Block(address) == 7) {
        return (Block(address) == 7 ? rom002_ : rom003_)[address & 0x3FF];
    }
    if (!isIo(address)) {
        return bus_;  // nothing selected: the bus keeps its charge
    }
    if (address & 4) {
        return 0;  // the timer: not modelled (read() flags it)
    }
    const std::size_t chip = (address >> 6) & 1 ? 0 : 1;  // A6: 1 the 002
    const Riot& r = riot_[chip];
    const unsigned reg = address & 3;
    if (reg == 1 || reg == 3) {
        return r.reg[reg];
    }
    const std::uint8_t pins = chip == 0 ? (reg == 0 ? paPins() : pbPins())
                                        : static_cast<std::uint8_t>(r.reg[reg] | ~r.reg[reg + 1]);
    // An output reads its data bit, an input its pin.
    return static_cast<std::uint8_t>((r.reg[reg] & r.reg[reg + 1]) | (pins & ~r.reg[reg + 1]));
}

std::uint8_t Bw1Model::read(std::uint16_t address) {
    if (isIo(address) && (address & 4)) {
        timerTouched_ = true;
    }
    bus_ = peek(address);
    return bus_;
}

void Bw1Model::write(std::uint16_t address, std::uint8_t value) {
    bus_ = value;
    if (const auto c = cell(address)) {
        cells_[*c] = static_cast<std::uint8_t>((value & ~stuckMask_[*c]) | stuckValue_[*c]);
    } else if (isIo(address)) {
        const std::size_t chip = (address >> 6) & 1 ? 0 : 1;
        if (address & 4) {
            timerTouched_ = true;
        } else {
            riot_[chip].reg[address & 3] = value;
            if (chip == 0) {
                displayChanged();
            }
        }
    }
}

void Bw1Model::poke(std::uint16_t address, std::uint8_t value) {
    const auto c = cell(address);
    if (!c) {
        throw std::invalid_argument("Bw1Model::poke: not RAM");
    }
    cells_[*c] = static_cast<std::uint8_t>((value & ~stuckMask_[*c]) | stuckValue_[*c]);
}

void Bw1Model::stick(std::uint16_t address, unsigned bit, bool one) {
    const auto c = cell(address);
    if (!c || bit > 7) {
        throw std::invalid_argument("Bw1Model::stick: not a RAM bit");
    }
    const auto b = static_cast<std::uint8_t>(1u << bit);
    stuckMask_[*c] = static_cast<std::uint8_t>(stuckMask_[*c] | b);
    stuckValue_[*c] = static_cast<std::uint8_t>(one ? stuckValue_[*c] | b : stuckValue_[*c] & ~b);
    poke(address, cells_[*c]);
}

void Bw1Model::patchRom(std::uint16_t address, std::span<const std::uint8_t> bytes) {
    for (const std::uint8_t byte : bytes) {
        const int block = Block(address);
        if (block != 6 && block != 7) {
            throw std::invalid_argument("Bw1Model::patchRom: not ROM");
        }
        (block == 7 ? rom002_ : rom003_)[address & 0x3FF] = byte;
        ++address;
    }
}

void Bw1Model::displayChanged() {
    const std::uint64_t elapsed = cycle_ - stateStart_;
    if (digit_ >= 0) {
        const auto d = static_cast<std::size_t>(digit_);
        digitOn_[d] += elapsed;
        for (std::size_t s = 0; s < 7; ++s) {
            if (segments_ & (1u << s)) {
                segmentOn_[d][s] += elapsed;
            }
        }
    }
    stateStart_ = cycle_;
    const int n = u24();
    digit_ = n >= 4 && n <= 9 ? n - 4 : -1;
    segments_ = static_cast<std::uint8_t>(paPins() & 0x7F);  // a segment lit while its PA is high
}

}  // namespace bw
