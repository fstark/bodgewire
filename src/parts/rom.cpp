#include "parts/rom.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <utility>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

// The compile-time part of a RomSpec: evaluate() is specialised on it, its loops unrolled.
template <std::size_t A, std::size_t D, std::size_t E>
struct RomLayout {
    std::array<PinIndex, A> address;
    std::array<Time, A> addressDelay;
    std::array<PinIndex, D> data;
    std::array<RomEnable, E> enables;
    Time offDelay;

    constexpr Time longestAddressDelay() const { return std::ranges::max(addressDelay); }
    constexpr bool oneAddressDelay() const {
        return std::ranges::min(addressDelay) == std::ranges::max(addressDelay);
    }
};

// Data bits on which every word the address could select agrees, the `unknown` address bits
// (X or floating) taking every value. More than 8 unknown bits: none.
unsigned KnownBits(std::span<const std::uint8_t> words, std::size_t base, std::size_t unknown) {
    if (std::popcount(unknown) > 8) {
        return 0;
    }
    const unsigned value = words[base];
    unsigned known = 0xFFu;
    for (std::size_t subset = unknown; subset != 0; subset = (subset - 1) & unknown) {
        known &= ~(words[base | subset] ^ value);
    }
    return known;
}

template <const auto& L>
void EvaluateRom(EvalContext& ctx, std::span<const std::uint8_t> words, unsigned activeHigh,
                 RomState& state) {
    Time& dataReady = state.dataReady;
    if (state.quiet) {
        // Woken by an enable after a quiet spell: the address changes it slept through set when
        // the word is ready, as they did when every one of them woke the chip.
        state.quiet = false;
        Time last = -1;
        for (const PinIndex pin : L.address) {
            ctx.quiet(pin, false);
            last = std::max(last, ctx.lastChange(pin));
        }
        if (last < 0) {
            dataReady = 0;
        } else if constexpr (L.oneAddressDelay()) {
            dataReady = last + L.addressDelay[0];
        } else {
            Time delay = 0;
            for (std::size_t i = 0; i < L.address.size(); ++i) {
                if (ctx.lastChange(L.address[i]) == last) {
                    delay = std::max(delay, L.addressDelay[i]);
                }
            }
            dataReady = last + delay;
        }
    }

    bool off = false;
    bool unknownEnable = false;
    bool enableChanged = false;
    Time enableDelay = 0;
    for (std::size_t i = 0; i < L.enables.size(); ++i) {
        const RomEnable& enable = L.enables[i];
        const LogicValue v = ctx.read(enable.pin);
        off |= v == ((activeHigh >> i) & 1u ? Zero : One);
        unknownEnable |= v != One && v != Zero;
        if (v != ctx.readBefore(enable.pin)) {
            enableChanged = true;
            enableDelay = std::max(enableDelay, enable.delay);
        }
    }

    std::size_t base = 0;
    std::size_t unknown = 0;
    bool addressChanged = false;
    Time addressDelay = 0;
    for (std::size_t i = 0; i < L.address.size(); ++i) {
        const PinIndex pin = L.address[i];
        const LogicValue v = ctx.read(pin);
        base |= std::size_t{v == One} << i;
        unknown |= std::size_t{v != One && v != Zero} << i;
        if (v != ctx.readBefore(pin)) {
            addressChanged = true;
            if constexpr (!L.oneAddressDelay()) {
                addressDelay = std::max(addressDelay, L.addressDelay[i]);
            }
        }
    }
    if constexpr (L.oneAddressDelay()) {
        addressDelay = L.addressDelay[0];
    }
    if (addressChanged) {
        dataReady = ctx.now() + addressDelay;
    }

    const unsigned value = words[base];
    const unsigned known = unknown == 0 ? 0xFFu : KnownBits(words, base, unknown);

    Time delay = L.longestAddressDelay();  // first evaluation
    if (enableChanged && off) {
        delay = L.offDelay;
    } else if (addressChanged) {
        delay = addressDelay;
    } else if (enableChanged) {
        // Selected after the address moved: the later of select time and address access (P5v).
        delay = std::max(enableDelay, dataReady - ctx.now());
    }
    for (std::size_t b = 0; b < L.data.size(); ++b) {
        LogicValue out = X;
        if (off) {
            out = Z;
        } else if (!unknownEnable && ((known >> b) & 1)) {
            out = ((value >> b) & 1) ? One : Zero;
        }
        ctx.drive(L.data[b], out, delay);
    }
    if (off) {
        // Off for sure: nothing but an enable changes what the chip does until it is on again.
        state.quiet = true;
        for (const PinIndex pin : L.address) {
            ctx.quiet(pin, true);
        }
    }
}

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
constexpr RomLayout s_2716Layout{s_2716Address, s_2716AddressDelay, s_2716Data, s_2716Enables,
                                 delays::k2716Float};

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
constexpr RomLayout s_2513Layout{s_2513Address, s_2513AddressDelay, s_2513Data, s_2513Enables,
                                 delays::k2513ChipEnable};

// 2316B: 2K x 8 mask ROM (MOS 2316 p. 1; Synertek SY2316B), the 2716's pinout with three chip
// selects whose polarities are set in the mask (select=). Enables listed CS1, CS2, CS3.
constexpr std::array<std::string_view, 24> s_2316Names{
    "A7", "A6", "A5", "A4", "A3", "A2",  "A1",  "A0",  "O0",  "O1", "O2", "GND",
    "O3", "O4", "O5", "O6", "O7", "CS2", "A10", "CS1", "CS3", "A9", "A8", "VCC"};
constexpr std::array<Time, 11> s_2316AddressDelay{
    delays::k2316Access, delays::k2316Access, delays::k2316Access, delays::k2316Access,
    delays::k2316Access, delays::k2316Access, delays::k2316Access, delays::k2316Access,
    delays::k2316Access, delays::k2316Access, delays::k2316Access};
constexpr std::array<RomEnable, 3> s_2316Enables{
    {{19, delays::k2316Select}, {17, delays::k2316Select}, {20, delays::k2316Select}}};
constexpr RomLayout s_2316Layout{s_2716Address, s_2316AddressDelay, s_2716Data, s_2316Enables,
                                 delays::k2316Float};

// 2332: 4K x 8 mask ROM (MOS 2332; the ZX80's IC2, "8332"), the 2316B's pins with A11 on pin 18
// and two chip selects whose polarities are set in the mask (select=). Enables listed CS1, CS2.
constexpr std::array<std::string_view, 24> s_2332Names{
    "A7", "A6", "A5", "A4", "A3", "A2",  "A1",  "A0",  "O0",  "O1", "O2", "GND",
    "O3", "O4", "O5", "O6", "O7", "A11", "A10", "CS1", "CS2", "A9", "A8", "VCC"};
constexpr std::array<PinIndex, 12> s_2332Address{7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18, 17};
constexpr std::array<Time, 12> s_2332AddressDelay{
    delays::k2332Access, delays::k2332Access, delays::k2332Access, delays::k2332Access,
    delays::k2332Access, delays::k2332Access, delays::k2332Access, delays::k2332Access,
    delays::k2332Access, delays::k2332Access, delays::k2332Access, delays::k2332Access};
constexpr std::array<RomEnable, 2> s_2332Enables{
    {{19, delays::k2332Select}, {20, delays::k2332Select}}};
constexpr RomLayout s_2332Layout{s_2332Address, s_2332AddressDelay, s_2716Data, s_2332Enables,
                                 delays::k2332Float};

const std::array<RomSpec, 4> s_specs{{
    {"2513",
     s_2513Names,
     s_2513Address,
     s_2513AddressDelay,
     s_2513Data,
     s_2513Enables,
     {},
     delays::k2513ChipEnable,
     &EvaluateRom<s_2513Layout>},
    {"2716", s_2716Names, s_2716Address, s_2716AddressDelay, s_2716Data, s_2716Enables, s_2716Other,
     delays::k2716Float, &EvaluateRom<s_2716Layout>},
    {"2316B",
     s_2316Names,
     s_2716Address,
     s_2316AddressDelay,
     s_2716Data,
     s_2316Enables,
     {},
     delays::k2316Float,
     &EvaluateRom<s_2316Layout>,
     true},
    {"2332",
     s_2332Names,
     s_2332Address,
     s_2332AddressDelay,
     s_2716Data,
     s_2332Enables,
     {},
     delays::k2332Float,
     &EvaluateRom<s_2332Layout>,
     true},
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

Rom::Rom(std::string reference, const RomSpec& spec, std::vector<std::uint8_t> words,
         unsigned activeHigh)
    : Component(std::move(reference), RomPins(spec)), spec_(spec), block_(std::move(words)),
      activeHigh_(activeHigh) {
    Check(block_.size() == std::size_t{1} << spec.address.size(), "rom: wrong number of words");
}

void Rom::powerOn(PowerOnContext& /*ctx*/) { state_ = {}; }  // the Simulator made every Pin loud

void Rom::evaluate(EvalContext& ctx) { spec_.evaluate(ctx, block_.words(), activeHigh_, state_); }

std::size_t Rom::wordCount(std::size_t memory) const {
    Check(memory == 0, "rom: one memory");
    return block_.size();
}

std::optional<unsigned> Rom::dataBit(PinIndex pin) const {
    const auto it = std::ranges::find(spec_.data, pin);
    if (it == spec_.data.end()) {
        return std::nullopt;
    }
    return static_cast<unsigned>(it - spec_.data.begin());
}

void Rom::stick(std::size_t memory, std::size_t word, unsigned bit, bool one) {
    Check(memory == 0 && bit < spec_.data.size(), "rom: no such cell");
    block_.stick(word, bit, one);
}

std::uint8_t Rom::read(std::size_t memory, std::size_t word) const {
    Check(memory == 0 && word < block_.size(), "rom: no such word");
    return block_[word];
}

unsigned Rom::wordBits(std::size_t memory) const {
    Check(memory == 0, "rom: one memory");
    return static_cast<unsigned>(spec_.data.size());
}

}  // namespace bw
