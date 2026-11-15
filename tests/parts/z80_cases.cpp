#include "z80_cases.hpp"

#include <charconv>
#include <format>
#include <fstream>
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

// -1 for '-'.
std::optional<int> HexOrNone(std::string_view text) {
    if (text == "-") {
        return -1;
    }
    const auto value = Hex(text);
    return value ? std::optional<int>(static_cast<int>(*value)) : std::nullopt;
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

constexpr std::size_t kFields = 25;

bool ParseState(std::string_view line, Z80Registers& r, std::vector<Z80Case::Cell>& ram) {
    const auto words = Split(line, ' ');
    if (words.size() < kFields + 1) {
        return false;
    }
    std::array<unsigned, kFields> v{};
    for (std::size_t i = 0; i < kFields; ++i) {
        const auto value = Hex(words[i + 1]);
        if (!value) {
            return false;
        }
        v[i] = *value;
    }
    const auto b8 = [](unsigned x) { return static_cast<std::uint8_t>(x); };
    const auto b16 = [](unsigned x) { return static_cast<std::uint16_t>(x); };
    r.pc = b16(v[0]);
    r.sp = b16(v[1]);
    r.a = b8(v[2]);
    r.f = b8(v[3]);
    r.b = b8(v[4]);
    r.c = b8(v[5]);
    r.d = b8(v[6]);
    r.e = b8(v[7]);
    r.h = b8(v[8]);
    r.l = b8(v[9]);
    r.i = b8(v[10]);
    r.r = b8(v[11]);
    r.ix = b16(v[12]);
    r.iy = b16(v[13]);
    r.wz = b16(v[14]);
    r.af2 = b16(v[15]);
    r.bc2 = b16(v[16]);
    r.de2 = b16(v[17]);
    r.hl2 = b16(v[18]);
    r.im = b8(v[19]);
    r.iff1 = v[20] != 0;
    r.iff2 = v[21] != 0;
    r.ei = v[22] != 0;
    r.p = v[23] != 0;
    r.q = b8(v[24]);
    for (std::size_t i = kFields + 1; i < words.size(); ++i) {
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

bool ParseTStates(std::string_view line, std::vector<Z80Case::TState>& out) {
    const auto words = Split(line, ' ');
    for (std::size_t i = 1; i < words.size(); ++i) {
        const auto parts = Split(words[i], ':');
        if (parts.size() != 3 || parts[2].size() != 4) {
            return false;
        }
        const auto address = HexOrNone(parts[0]);
        const auto data = HexOrNone(parts[1]);
        if (!address || !data) {
            return false;
        }
        out.push_back({*address, *data, {parts[2][0], parts[2][1], parts[2][2], parts[2][3]}});
    }
    return true;
}

bool ParsePorts(std::string_view line, std::vector<Z80Case::Port>& out) {
    const auto words = Split(line, ' ');
    for (std::size_t i = 1; i < words.size(); ++i) {
        const auto parts = Split(words[i], ':');
        if (parts.size() != 3 || (parts[2] != "r" && parts[2] != "w")) {
            return false;
        }
        const auto address = Hex(parts[0]);
        const auto value = Hex(parts[1]);
        if (!address || !value) {
            return false;
        }
        out.push_back({static_cast<std::uint16_t>(*address), static_cast<std::uint8_t>(*value),
                       parts[2] == "r"});
    }
    return true;
}

std::string Show(const Z80Case::TState& t) {
    const auto hex = [](int v) { return v < 0 ? std::string("-") : std::format("{:x}", v); };
    return std::format("{}:{}:{}", hex(t.address), hex(t.data),
                       std::string_view(t.flags.data(), t.flags.size()));
}

std::string Describe(const Z80Registers& r) {
    return std::format("pc {:04x} sp {:04x} af {:02x}{:02x} bc {:02x}{:02x} de {:02x}{:02x} hl "
                       "{:02x}{:02x} ix {:04x} iy {:04x} wz {:04x} i {:02x} r {:02x} af' {:04x} "
                       "bc' {:04x} de' {:04x} hl' {:04x} im {} iff {}{} ei {} p {} q {:02x}",
                       r.pc, r.sp, r.a, r.f, r.b, r.c, r.d, r.e, r.h, r.l, r.ix, r.iy, r.wz, r.i,
                       r.r, r.af2, r.bc2, r.de2, r.hl2, r.im, int(r.iff1), int(r.iff2), int(r.ei),
                       int(r.p), r.q);
}

bool Same(const Z80Registers& x, const Z80Registers& y) {
    return x.pc == y.pc && x.sp == y.sp && x.ix == y.ix && x.iy == y.iy && x.wz == y.wz &&
           x.a == y.a && x.f == y.f && x.b == y.b && x.c == y.c && x.d == y.d && x.e == y.e &&
           x.h == y.h && x.l == y.l && x.i == y.i && x.r == y.r && x.af2 == y.af2 &&
           x.bc2 == y.bc2 && x.de2 == y.de2 && x.hl2 == y.hl2 && x.im == y.im && x.q == y.q &&
           x.iff1 == y.iff1 && x.iff2 == y.iff2 && x.ei == y.ei && x.p == y.p;
}

// The T-states of one M-cycle as Harte's data samples them ("simplified memory access").
void Expand(const Z80Bus& bus, int data, int& last, std::vector<Z80Case::TState>& out) {
    using enum Z80CycleKind;
    const int a = bus.address;
    const auto t = [&out](int address, int value, const char* flags) {
        out.push_back({address, value, {flags[0], flags[1], flags[2], flags[3]}});
    };
    switch (bus.kind) {
    case Fetch:
        t(a, -1, "----");
        t(a, -1, "r-m-");
        t(bus.refresh, data, "----");
        t(bus.refresh, -1, "----");
        last = bus.refresh;
        return;
    case Read:
        t(a, -1, "----");
        t(a, -1, "r-m-");
        t(a, data, "----");
        break;
    case Write:
        t(a, -1, "----");
        t(a, bus.data, "-wm-");
        t(a, -1, "----");
        break;
    case In:
        t(a, -1, "----");
        t(a, -1, "----");
        t(a, -1, "r--i");
        t(a, data, "----");
        break;
    case Out:
        t(a, -1, "----");
        t(a, -1, "----");
        t(a, bus.data, "-w-i");
        t(a, -1, "----");
        break;
    case Acknowledge:
        for (int i = 0; i < 6; ++i) {
            t(a, -1, "????");  // never in Harte's data: fails the comparison
        }
        last = bus.refresh;
        return;
    case Internal:
        for (int i = 0; i < bus.length; ++i) {
            t(last, -1, "----");
        }
        return;
    }
    last = a;
}

bool Matches(const Z80Case::TState& x, const Z80Case::TState& y) {
    return x.address == y.address && x.data == y.data && x.flags == y.flags;
}

}  // namespace

std::expected<std::vector<Z80Case>, std::string> ReadZ80Cases(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(std::format("{}: cannot open", path.string()));
    }
    std::vector<Z80Case> cases;
    std::string line, name;
    int number = 0;
    const auto bad = [&] { return std::unexpected(std::format("{}:{}", path.string(), number)); };
    while (std::getline(in, line)) {
        ++number;
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            name = line.size() > 2 ? line.substr(2) : "";
            continue;
        }
        if (line[0] == 'I') {
            Z80Case c;
            c.name = name;
            if (!ParseState(line, c.initial, c.initialRam)) {
                return bad();
            }
            cases.push_back(std::move(c));
            continue;
        }
        if (cases.empty()) {
            return bad();
        }
        Z80Case& c = cases.back();
        const bool ok = line[0] == 'F'   ? ParseState(line, c.final, c.finalRam)
                        : line[0] == 'C' ? ParseTStates(line, c.tStates)
                        : line[0] == 'P' ? ParsePorts(line, c.ports)
                                         : false;
        if (!ok) {
            return bad();
        }
    }
    return cases;
}

std::optional<std::string> RunZ80Case(const Z80Case& c) {
    static std::array<std::uint8_t, 0x10000> ram{};
    for (const auto& cell : c.initialRam) {
        ram[cell.address] = cell.value;
    }
    const auto clear = [&c] {
        for (const auto& cell : c.initialRam) {
            ram[cell.address] = 0;
        }
        for (const auto& cell : c.finalRam) {
            ram[cell.address] = 0;
        }
    };
    const auto fail = [&](std::string what) {
        clear();
        return std::optional<std::string>(std::format("{}: {}", c.name, what));
    };

    Z80Core core;
    core.start(c.initial);
    std::size_t at = 0, port = 0;
    int last = -1;
    std::vector<Z80Case::TState> expanded;
    while (at < c.tStates.size()) {
        const Z80Bus bus = core.bus();
        int data = -1;
        switch (bus.kind) {
        case Z80CycleKind::Fetch:
        case Z80CycleKind::Read:
            data = ram[bus.address];
            break;
        case Z80CycleKind::Write:
            ram[bus.address] = bus.data;
            break;
        case Z80CycleKind::In:
        case Z80CycleKind::Out:
            if (port >= c.ports.size() || c.ports[port].address != bus.address ||
                c.ports[port].read != (bus.kind == Z80CycleKind::In)) {
                return fail(
                    std::format("T-state {}: unexpected port access at {:04x}", at, bus.address));
            }
            if (bus.kind == Z80CycleKind::In) {
                data = c.ports[port].value;
            } else if (c.ports[port].value != bus.data) {
                return fail(std::format("T-state {}: OUT {:02x}, expected {:02x}", at, bus.data,
                                        c.ports[port].value));
            }
            ++port;
            break;
        default:
            break;
        }
        expanded.clear();
        Expand(bus, data, last, expanded);
        for (const auto& t : expanded) {
            if (at >= c.tStates.size()) {
                return fail(std::format("more T-states than {}: {}", c.tStates.size(), Show(t)));
            }
            if (!Matches(t, c.tStates[at])) {
                return fail(
                    std::format("T-state {}: {}, expected {}", at, Show(t), Show(c.tStates[at])));
            }
            ++at;
        }
        core.step(static_cast<std::uint8_t>(data < 0 ? 0xFF : data));
    }
    if (!core.instructionEnded()) {
        return fail("the instruction did not end with the listed T-states");
    }
    if (port != c.ports.size()) {
        return fail("a listed port access did not happen");
    }
    const Z80Registers r = core.registers();
    if (!Same(r, c.final)) {
        return fail(
            std::format("registers\n  got      {}\n  expected {}", Describe(r), Describe(c.final)));
    }
    for (const auto& cell : c.finalRam) {
        if (ram[cell.address] != cell.value) {
            return fail(std::format("RAM {:04x} = {:02x}, expected {:02x}", cell.address,
                                    ram[cell.address], cell.value));
        }
    }
    clear();
    return std::nullopt;
}

}  // namespace bw::test
