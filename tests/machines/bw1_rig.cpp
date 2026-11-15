#include "bw1_rig.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "bw1_model.hpp"
#include "parts/switch.hpp"

namespace bw {

using namespace literals;
using enum LogicValue;

namespace {

constexpr Time kHold = 20_ms;  // a key held, then released as long: two scans of 3.5 ms each
constexpr Time kPoll = 20_us;  // a digit is selected about 550 us a scan

NetId PinNetOf(const LoadedCircuit& loaded, const std::string& reference, std::string_view pin) {
    const auto net = PinNet(loaded, reference, pin);
    REQUIRE(net.has_value());
    return *net;
}

}  // namespace

std::filesystem::path Bw1NetlistPath() { return std::filesystem::path(BW_NETLIST_DIR) / "bw1.bwn"; }

std::string Bw1KeyReference(std::string_view name) {
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

Bw1::Bw1(std::unique_ptr<LoadedCircuit> loaded, std::uint64_t seed)
    : loaded_(std::move(loaded)), sim_(loaded_->circuit) {
    for (int k = 1; k <= 24; ++k) {
        Switch* sw = SwitchOf(*loaded_, "SW" + std::to_string(k));
        REQUIRE(sw != nullptr);
        sw->setPressed(false);
    }
    for (int d = 0; d < 6; ++d) {
        const std::string digit = "DS" + std::to_string(d + 1);
        digits_[d] = PinNetOf(*loaded_, digit, "CC1");
        for (int s = 0; s < 7; ++s) {
            segments_[d][s] = PinNetOf(*loaded_, digit, std::string(1, static_cast<char>('A' + s)));
        }
    }
    sim_.powerOn(seed);
}

void Bw1::set(std::string_view reference, bool pressed) {
    Switch* sw = SwitchOf(*loaded_, reference);
    REQUIRE(sw != nullptr);
    sw->setPressed(pressed);
    sim_.wake(sw->id());
}

void Bw1::press(std::string_view key) {
    hold(key);
    run(kHold);
    release(key);
    run(kHold);
}

void Bw1::type(std::string_view keys) {
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

std::string Bw1::display(Time window) {
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
            const NetId net = segments_[selected][s];
            if (sim_.value(net) == One && !sim_.isFloating(net)) {
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

std::array<std::uint8_t, 6> Bw1::segments(Time window) {
    std::array<int, 6> digitOn{};
    std::array<std::array<int, 7>, 6> segmentOn{};
    const Time end = sim_.now() + window;
    while (sim_.now() < end) {
        run(kPoll);
        for (int d = 0; d < 6; ++d) {
            if (sim_.value(digits_[d]) != Zero || sim_.isFloating(digits_[d])) {
                continue;
            }
            ++digitOn[d];
            for (int s = 0; s < 7; ++s) {
                const NetId net = segments_[d][s];
                if (sim_.value(net) == One && !sim_.isFloating(net)) {
                    ++segmentOn[d][s];
                }
            }
        }
    }
    std::array<std::uint8_t, 6> lit{};
    for (int d = 0; d < 6; ++d) {
        for (int s = 0; s < 7; ++s) {
            if (digitOn[d] > 0 && segmentOn[d][s] * 4 >= digitOn[d]) {
                lit[d] = static_cast<std::uint8_t>(lit[d] | (1u << s));
            }
        }
    }
    return lit;
}

bool IsHex(std::string_view text) {
    return !text.empty() && std::ranges::all_of(text, [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || (c >= 'A' && c <= 'F');
    });
}

void CheckShows(const std::string& shown, std::string_view address) {
    INFO(shown);
    CHECK(shown.substr(0, 4) == address);
    CHECK(IsHex(shown.substr(4)));
}

Bw1 Started(std::unique_ptr<LoadedCircuit> loaded, std::uint64_t seed) {
    Bw1 bw1(std::move(loaded), seed);
    bw1.runUntil(40_ms);
    return bw1;
}

std::unique_ptr<LoadedCircuit> Bw1Circuit(Machine& machine, bool strap) {
    const PinRef strapPad{"U24", "4"};
    const PinRef pa0{"U2", "2"};
    if (strap) {
        REQUIRE(machine.addWire(strapPad, pa0));
    }
    auto built = machine.build();
    if (strap) {
        REQUIRE(machine.removeWire(strapPad, pa0));
    }
    REQUIRE(built.has_value());
    return std::move(*built);
}

}  // namespace bw
