#include "parts/rom.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

// 2716: 2K x 8 EPROM (Intel pinout).
constexpr std::array<std::string_view, 24> s_2716Names{
    "A7", "A6", "A5", "A4", "A3", "A2",   "A1",  "A0",   "O0",  "O1", "O2", "GND",
    "O3", "O4", "O5", "O6", "O7", "CE_N", "A10", "OE_N", "VPP", "A9", "A8", "VCC"};
constexpr std::array<PinIndex, 11> s_2716Address{7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18};
constexpr std::array<Time, 11> s_2716AddressDelay{
    delays::k2716Access, delays::k2716Access, delays::k2716Access, delays::k2716Access,
    delays::k2716Access, delays::k2716Access, delays::k2716Access, delays::k2716Access,
    delays::k2716Access, delays::k2716Access, delays::k2716Access};
constexpr std::array<PinIndex, 8> s_2716Data{8, 9, 10, 12, 13, 14, 15, 16};
constexpr std::array<RomEnable, 2> s_2716Enables{
    {{17, delays::k2716ChipEnable}, {19, delays::k2716OutputEnable}}};
constexpr std::array<PinIndex, 1> s_2716Other{20};

// 2513: 64 x 8 x 5 character generator (Signetics 1972 MOS data book, p. 65).
constexpr std::array<std::string_view, 24> s_2513Names{
    "VGG",  "NC2", "NC3", "O1", "O2", "O3", "O4", "O5", "NC9", "GND", "CE_N", "VDD",
    "NC13", "A1",  "A2",  "A3", "A4", "A5", "A6", "A7", "A8",  "A9",  "NC23", "VCC"};
constexpr std::array<PinIndex, 9> s_2513Address{13, 14, 15, 16, 17, 18, 19, 20, 21};
constexpr std::array<Time, 9> s_2513AddressDelay{
    delays::k2513RowAccess,       delays::k2513RowAccess,       delays::k2513RowAccess,
    delays::k2513CharacterAccess, delays::k2513CharacterAccess, delays::k2513CharacterAccess,
    delays::k2513CharacterAccess, delays::k2513CharacterAccess, delays::k2513CharacterAccess};
constexpr std::array<PinIndex, 5> s_2513Data{3, 4, 5, 6, 7};
constexpr std::array<RomEnable, 1> s_2513Enables{{{10, delays::k2513ChipEnable}}};

const std::array<RomSpec, 2> s_specs{{
    {"2513",
     s_2513Names,
     s_2513Address,
     s_2513AddressDelay,
     s_2513Data,
     s_2513Enables,
     {},
     delays::k2513ChipEnable},
    {"2716", s_2716Names, s_2716Address, s_2716AddressDelay, s_2716Data, s_2716Enables, s_2716Other,
     delays::k2716Float},
}};

bool Listed(std::span<const PinIndex> pins, PinIndex pin) {
    return std::ranges::find(pins, pin) != pins.end();
}

std::vector<PinSpec> RomPins(const RomSpec& spec) {
    std::vector<PinSpec> pins;
    for (PinIndex i = 0; i < spec.names.size(); ++i) {
        const std::string_view name = spec.names[i];
        PinKind kind = PinKind::NoConnect;
        if (name == "VCC" || name == "GND") {
            kind = PinKind::Power;
        } else if (Listed(spec.data, i)) {
            kind = PinKind::Output;
        } else if (Listed(spec.address, i) || Listed(spec.otherInputs, i) ||
                   std::ranges::any_of(spec.enables,
                                       [i](const RomEnable& e) { return e.pin == i; })) {
            kind = PinKind::Input;
        }
        pins.push_back(PinSpec{std::string(name), kind, Family::Ttl});
    }
    return pins;
}

}  // namespace

const RomSpec* FindRomSpec(std::string_view part) {
    for (const RomSpec& spec : s_specs) {
        if (spec.part == part) {
            return &spec;
        }
    }
    return nullptr;
}

const RomSpec& RomSpecOf(std::string_view part) {
    const RomSpec* spec = FindRomSpec(part);
    Check(spec != nullptr, "rom: unknown part " + std::string(part));
    return *spec;
}

Rom::Rom(std::string reference, const RomSpec& spec, std::vector<std::uint8_t> words)
    : Component(std::move(reference), RomPins(spec)), spec_(spec), words_(std::move(words)) {
    Check(words_.size() == std::size_t{1} << spec.address.size(), "rom: wrong number of words");
    Check(spec.data.size() <= 8 && spec.addressDelay.size() == spec.address.size(),
          "rom: bad spec");
}

void Rom::evaluate(EvalContext& ctx) {
    bool off = false;
    bool unknownEnable = false;
    bool enableChanged = false;
    Time enableDelay = 0;
    for (const RomEnable& enable : spec_.enables) {
        const LogicValue v = ctx.read(enable.pin);
        if (v == One) {
            off = true;
        } else if (v != Zero) {
            unknownEnable = true;
        }
        if (v != ctx.readBefore(enable.pin)) {
            enableChanged = true;
            enableDelay = std::max(enableDelay, enable.delay);
        }
    }

    std::size_t base = 0;
    std::array<std::size_t, 8> unknownBits{};
    std::size_t unknownCount = 0;
    bool tooMany = false;
    bool addressChanged = false;
    Time addressDelay = 0;
    Time longestAddressDelay = 0;
    for (std::size_t i = 0; i < spec_.address.size(); ++i) {
        const PinIndex pin = spec_.address[i];
        const LogicValue v = ctx.read(pin);
        if (v == One) {
            base |= std::size_t{1} << i;
        } else if (v != Zero) {
            if (unknownCount < unknownBits.size()) {
                unknownBits[unknownCount++] = std::size_t{1} << i;
            } else {
                tooMany = true;
            }
        }
        if (v != ctx.readBefore(pin)) {
            addressChanged = true;
            addressDelay = std::max(addressDelay, spec_.addressDelay[i]);
        }
        longestAddressDelay = std::max(longestAddressDelay, spec_.addressDelay[i]);
    }

    // Data bits on which every word the address could select agrees.
    const unsigned value = words_[base];
    unsigned known = tooMany ? 0u : 0xFFu;
    for (std::size_t combo = 1; !tooMany && combo < (std::size_t{1} << unknownCount); ++combo) {
        std::size_t address = base;
        for (std::size_t b = 0; b < unknownCount; ++b) {
            if ((combo >> b) & 1) {
                address |= unknownBits[b];
            }
        }
        known &= ~(words_[address] ^ value);
    }

    Time delay = longestAddressDelay;  // first evaluation
    if (enableChanged && off) {
        delay = spec_.offDelay;
    } else if (addressChanged) {
        delay = addressDelay;
    } else if (enableChanged) {
        delay = enableDelay;
    }
    for (std::size_t b = 0; b < spec_.data.size(); ++b) {
        LogicValue out = X;
        if (off) {
            out = Z;
        } else if (!unknownEnable && ((known >> b) & 1)) {
            out = ((value >> b) & 1) ? One : Zero;
        }
        ctx.drive(spec_.data[b], out, delay);
    }
}

}  // namespace bw
