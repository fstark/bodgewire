#include "bw2001_model.hpp"

#include <algorithm>
#include <filesystem>
#include <random>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "parts/rom_contents.hpp"

namespace bw {

namespace {

constexpr std::uint64_t kRetraceCycles = 16'667;  // 60 Hz at 1 MHz
constexpr std::uint8_t kCrbFlag = 0x80, kCrbEnable = 0x01, kSelectOr = 0x04;

std::vector<std::uint8_t> LoadRom(std::string_view name, std::size_t size) {
    const auto words = ReadRomContents(std::filesystem::path(BW_ROM_DIR) / name, size, 8);
    if (!words) {
        FAIL(words.error());
    }
    return *words;
}

}  // namespace

Bw2001Model::Bw2001Model(Options options)
    : ramTop_(std::min<std::uint32_t>(options.ramTop, 0x2000)), ram_(0x2000), ramMask_(0x2000),
      ramValue_(0x2000), diag_(options.diag) {
    if (options.roms.empty()) {
        options.roms[0xC000] = LoadRom("bw2001_basic.rom", 0x2000);
        options.roms[0xE000] = LoadRom("bw2001_edit.rom", 0x800);
        options.roms[0xF000] = LoadRom("bw2001_kernel.rom", 0x1000);
    }
    for (auto& [base, words] : options.roms) {
        Window window{base, std::move(words), {}};
        for (std::size_t chip = 0; chip < window.words.size(); chip += 0x800) {
            const auto begin = window.words.begin() + static_cast<std::ptrdiff_t>(chip);
            const auto end =
                window.words.begin() +
                static_cast<std::ptrdiff_t>(std::min(chip + 0x800, window.words.size()));
            window.filled.push_back(
                std::any_of(begin, end, [](std::uint8_t w) { return w != 0xFF; }));
        }
        roms_.push_back(std::move(window));
    }
    std::mt19937_64 random(options.seed);
    for (auto& byte : ram_) {
        byte = static_cast<std::uint8_t>(random());
    }
    for (auto& byte : video_) {
        byte = static_cast<std::uint8_t>(random());
    }
    core_.start({.pc = static_cast<std::uint16_t>(random()),
                 .a = static_cast<std::uint8_t>(random()),
                 .x = static_cast<std::uint8_t>(random()),
                 .y = static_cast<std::uint8_t>(random()),
                 .s = static_cast<std::uint8_t>(random()),
                 .p = static_cast<std::uint8_t>(random())});
    core_.setIrq(false);
    reset();
}

void Bw2001Model::reset() {
    core_.setReset(true);
    ora_ = ddra_ = cra_ = orb_ = ddrb_ = crb_ = 0;
    core_.setIrq(false);
    for (int i = 0; i < 8; ++i) {
        cycle();
    }
    core_.setReset(false);
}

void Bw2001Model::run(std::uint64_t us) {
    for (std::uint64_t i = 0; i < us; ++i) {
        cycle();
    }
}

void Bw2001Model::cycle() {
    if (cycle_ % kRetraceCycles == kRetraceCycles - 1) {
        crb_ |= kCrbFlag;  // CB1's fall (the firmware sets the negative edge)
    }
    core_.setIrq((crb_ & kCrbFlag) != 0 && (crb_ & kCrbEnable) != 0);
    const Mos6502Bus bus = core_.bus();
    if (!bus.read && (bus.address >> 8) == 0x01) {  // a stack write
        stackMark_ = std::min(stackMark_, static_cast<std::uint8_t>(bus.address & 0xFF));
    }
    if (bus.read) {
        bus_ = read(bus.address);
        core_.step(bus_);
    } else {
        bus_ = bus.data;
        write(bus.address, bus.data);
        core_.step(0);
    }
    ++cycle_;
}

void Bw2001Model::setKey(Bw2001Key key, bool held) {
    if (key.row < 0 || key.row > 9 || key.column < 0 || key.column > 7) {
        throw std::invalid_argument("Bw2001Model::setKey: row 0-9, column 0-7");
    }
    const auto bit = static_cast<std::uint8_t>(1u << key.column);
    held_[key.row] = held ? held_[key.row] | bit : held_[key.row] & ~bit;
}

const Bw2001Model::Window* Bw2001Model::windowOf(std::uint16_t address) const {
    for (const Window& w : roms_) {
        if (address >= w.base && address - w.base < w.words.size()) {
            return &w;
        }
    }
    return nullptr;
}

std::uint8_t Bw2001Model::ramRead(std::uint16_t address) const {
    if (dead_) {
        return 0xFF;
    }
    return (ram_[address] & ~ramMask_[address]) | (ramValue_[address] & ramMask_[address]);
}

std::uint8_t Bw2001Model::videoRead(std::uint16_t address) const {
    const std::size_t at = address & 0x3FF;
    return (video_[at] & ~videoMask_[at]) | (videoValue_[at] & videoMask_[at]);
}

std::uint8_t Bw2001Model::paPins() const {
    // Outputs from ORA; inputs pulled up, PA7 low while DIAG is.
    std::uint8_t pins = (ora_ & ddra_) | static_cast<std::uint8_t>(~ddra_);
    if (diag_ && (ddra_ & 0x80) == 0) {
        pins &= 0x7F;
    }
    return pins;
}

std::uint8_t Bw2001Model::pbPins() const {
    const int row = paPins() & 0x0F;  // UG9: rows 10-15 select nothing
    const std::uint8_t keys = row < 10 ? held_[static_cast<std::size_t>(row)] : 0;
    const std::uint8_t inputs = static_cast<std::uint8_t>(~keys);
    return (orb_ & ddrb_) | (inputs & static_cast<std::uint8_t>(~ddrb_));
}

std::uint8_t Bw2001Model::peek(std::uint16_t address) const {
    if (address < 0x2000) {
        return address < ramTop_ ? ramRead(address) : bus_;
    }
    if (address >= 0x8000 && address < 0x9000) {
        return videoRead(address);
    }
    if (address >= 0xE800 && address < 0xF000) {
        if ((address & 0x10) != 0) {
            switch (address & 3) {
            case 0:
                return (cra_ & kSelectOr) != 0 ? paPins() : ddra_;
            case 1:
                return cra_;
            case 2:
                return (crb_ & kSelectOr) != 0 ? pbPins() : ddrb_;
            default:
                return crb_;
            }
        }
        return bus_;
    }
    if (const Window* w = windowOf(address)) {
        return w->words[address - w->base];
    }
    return bus_;
}

std::uint8_t Bw2001Model::read(std::uint16_t address) {
    if (address >= 0xE800 && address < 0xF000) {
        if ((address & 0x60) != 0) {
            touched_ = true;  // PIA2 ($E820) or the VIA ($E840)
        }
        if ((address & 0x10) != 0 && (address & 3) == 2 && (crb_ & kSelectOr) != 0) {
            const std::uint8_t value = peek(address);
            crb_ &= static_cast<std::uint8_t>(~0xC0);  // reading ORB clears the flags
            return value;
        }
        return peek(address);
    }
    if (address >= 0xC000) {
        const Window* w = windowOf(address);
        if (w == nullptr || !w->filled[(address - w->base) / 0x800]) {
            touched_ = true;  // an empty socket
        }
    }
    return peek(address);
}

void Bw2001Model::write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x2000) {
        if (address < ramTop_) {
            ram_[address] = value;
        }
        return;
    }
    if (address >= 0x8000 && address < 0x9000) {
        video_[address & 0x3FF] = value;
        return;
    }
    if (address >= 0xE800 && address < 0xF000) {
        if ((address & 0x60) != 0) {
            touched_ = true;
        }
        if ((address & 0x10) != 0) {
            switch (address & 3) {
            case 0:
                ((cra_ & kSelectOr) != 0 ? ora_ : ddra_) = value;
                break;
            case 1:
                cra_ = (cra_ & 0xC0) | (value & 0x3F);
                break;
            case 2:
                ((crb_ & kSelectOr) != 0 ? orb_ : ddrb_) = value;
                break;
            default:
                crb_ = (crb_ & 0xC0) | (value & 0x3F);
                break;
            }
        }
    }
}

void Bw2001Model::poke(std::uint16_t address, std::uint8_t value) {
    if (address < 0x2000) {
        ram_[address] = value;
    } else if (address >= 0x8000 && address < 0x9000) {
        video_[address & 0x3FF] = value;
    } else {
        throw std::invalid_argument("Bw2001Model::poke: RAM or video RAM only");
    }
}

void Bw2001Model::stick(std::uint16_t address, unsigned bit, bool one) {
    const auto mask = static_cast<std::uint8_t>(1u << bit);
    if (address < 0x2000) {
        ramMask_[address] |= mask;
        ramValue_[address] = one ? ramValue_[address] | mask : ramValue_[address] & ~mask;
    } else if (address >= 0x8000 && address < 0x8400) {
        const std::size_t at = address & 0x3FF;
        videoMask_[at] |= mask;
        videoValue_[at] = one ? videoValue_[at] | mask : videoValue_[at] & ~mask;
    } else {
        throw std::invalid_argument("Bw2001Model::stick: RAM or video RAM only");
    }
}

void Bw2001Model::patchRom(std::uint16_t address, std::span<const std::uint8_t> bytes) {
    for (const std::uint8_t byte : bytes) {
        bool done = false;
        for (Window& w : roms_) {
            if (address >= w.base && address - w.base < w.words.size()) {
                w.words[address - w.base] = byte;
                done = true;
            }
        }
        if (!done) {
            throw std::invalid_argument("Bw2001Model::patchRom: no ROM there");
        }
        ++address;
    }
}

}  // namespace bw
