#include "mos6502_cases.hpp"

#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <sstream>
#include <string_view>

namespace bw::test {

namespace {

std::optional<unsigned> Hex(std::string_view text) {
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::string_view> Split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(separator, start);
        if (end == std::string_view::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

bool ParseState(std::string_view line, Mos6502Registers& r, std::vector<Mos6502Case::Cell>& ram) {
    const auto words = Split(line, ' ');
    if (words.size() < 7) {
        return false;
    }
    std::array<unsigned, 6> v{};
    for (std::size_t i = 0; i < 6; ++i) {
        const auto value = Hex(words[i + 1]);
        if (!value) {
            return false;
        }
        v[i] = *value;
    }
    r = {static_cast<std::uint16_t>(v[0]), static_cast<std::uint8_t>(v[2]),
         static_cast<std::uint8_t>(v[3]),  static_cast<std::uint8_t>(v[4]),
         static_cast<std::uint8_t>(v[1]),  static_cast<std::uint8_t>(v[5])};
    for (std::size_t i = 7; i < words.size(); ++i) {
        const auto pair = Split(words[i], ':');
        const auto address = pair.size() == 2 ? Hex(pair[0]) : std::nullopt;
        const auto value = pair.size() == 2 ? Hex(pair[1]) : std::nullopt;
        if (!address || !value) {
            return false;
        }
        ram.push_back({static_cast<std::uint16_t>(*address), static_cast<std::uint8_t>(*value)});
    }
    return true;
}

bool ParseCycles(std::string_view line, std::vector<Mos6502Case::Cycle>& cycles) {
    const auto words = Split(line, ' ');
    for (std::size_t i = 1; i < words.size(); ++i) {
        const auto triple = Split(words[i], ':');
        const auto address = triple.size() == 3 ? Hex(triple[0]) : std::nullopt;
        const auto value = triple.size() == 3 ? Hex(triple[1]) : std::nullopt;
        if (!address || !value || (triple[2] != "r" && triple[2] != "w")) {
            return false;
        }
        cycles.push_back({static_cast<std::uint16_t>(*address), static_cast<std::uint8_t>(*value),
                          triple[2] == "r"});
    }
    return true;
}

std::string Registers(const Mos6502Registers& r) {
    return std::format("pc {:04x} s {:02x} a {:02x} x {:02x} y {:02x} p {:02x}", r.pc, r.s, r.a,
                       r.x, r.y, r.p);
}

}  // namespace

std::expected<std::vector<Mos6502Case>, std::string>
ReadMos6502Cases(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(path.string() + ": cannot open");
    }
    std::vector<Mos6502Case> cases;
    std::string line, name;
    int number = 0;
    const auto error = [&](std::string_view what) {
        return std::unexpected(std::format("{}:{}: {}", path.string(), number, what));
    };
    while (std::getline(in, line)) {
        ++number;
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            name = line.size() > 2 ? line.substr(2) : std::string();
            continue;
        }
        if (line[0] == 'I') {
            cases.emplace_back();
            cases.back().name = name;
            if (!ParseState(line, cases.back().initial, cases.back().initialRam)) {
                return error("bad initial state");
            }
        } else if (line[0] == 'F' && !cases.empty()) {
            if (!ParseState(line, cases.back().final, cases.back().finalRam)) {
                return error("bad final state");
            }
        } else if (line[0] == 'C' && !cases.empty()) {
            if (!ParseCycles(line, cases.back().cycles)) {
                return error("bad cycles");
            }
        } else {
            return error("unknown line");
        }
    }
    return cases;
}

std::optional<std::string> RunMos6502Case(const Mos6502Case& c) {
    // One array for every case, cleared afterwards only where this case touched it.
    static std::vector<std::uint8_t> ram(0x10000, 0);
    struct Clear {
        const Mos6502Case& c;
        ~Clear() {
            for (const auto& cell : c.initialRam) {
                ram[cell.address] = 0;
            }
            for (const auto& cycle : c.cycles) {
                ram[cycle.address] = 0;
            }
        }
    } clear{c};
    for (const auto& cell : c.initialRam) {
        ram[cell.address] = cell.value;
    }
    Mos6502Core core;
    core.start(c.initial);
    for (std::size_t i = 0; i < c.cycles.size(); ++i) {
        const auto& want = c.cycles[i];
        const Mos6502Bus& bus = core.bus();
        if (bus.address != want.address || bus.read != want.read ||
            (!bus.read && bus.data != want.value)) {
            return std::format("{}: cycle {}: {:04x} {} {:02x}, expected {:04x} {} {:02x}", c.name,
                               i + 1, bus.address, bus.read ? 'r' : 'w', bus.data, want.address,
                               want.read ? 'r' : 'w', want.value);
        }
        if (bus.read) {
            core.step(ram[bus.address]);
        } else {
            ram[bus.address] = bus.data;
            core.step(0);
        }
    }
    const Mos6502Registers& r = core.registers();
    const Mos6502Registers& f = c.final;
    if (r.pc != f.pc || r.a != f.a || r.x != f.x || r.y != f.y || r.s != f.s || r.p != f.p) {
        return std::format("{}: registers {}, expected {}", c.name, Registers(r), Registers(f));
    }
    for (const auto& cell : c.finalRam) {
        if (ram[cell.address] != cell.value) {
            return std::format("{}: RAM {:04x} = {:02x}, expected {:02x}", c.name, cell.address,
                               ram[cell.address], cell.value);
        }
    }
    return std::nullopt;
}

}  // namespace bw::test
