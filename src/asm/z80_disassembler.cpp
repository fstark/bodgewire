#include "asm/z80_disassembler.hpp"

#include <array>
#include <format>
#include <string_view>

namespace bw {

namespace {

constexpr std::array<std::string_view, 8> kR{"B", "C", "D", "E", "H", "L", "(HL)", "A"};
constexpr std::array<std::string_view, 4> kRp{"BC", "DE", "HL", "SP"};
constexpr std::array<std::string_view, 4> kRp2{"BC", "DE", "HL", "AF"};
constexpr std::array<std::string_view, 8> kCc{"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
constexpr std::array<std::string_view, 8> kAlu{"ADD A,", "ADC A,", "SUB ", "SBC A,",
                                               "AND ",   "XOR ",   "OR ",  "CP "};
constexpr std::array<std::string_view, 8> kRot{"RLC", "RRC", "RL", "RR", "SLA", "SRA", "", "SRL"};
// Block instructions by [y - 4][z].
constexpr std::array<std::array<std::string_view, 4>, 4> kBlock{{{"LDI", "CPI", "INI", "OUTI"},
                                                                 {"LDD", "CPD", "IND", "OUTD"},
                                                                 {"LDIR", "CPIR", "INIR", "OTIR"},
                                                                 {"LDDR", "CPDR", "INDR", "OTDR"}}};

// The documented DD/FD opcodes: those naming (IX+d) or IX itself.
constexpr std::array<std::uint8_t, 39> kIndexed{
    0x09, 0x19, 0x21, 0x22, 0x23, 0x29, 0x2A, 0x2B, 0x34, 0x35, 0x36, 0x39, 0x46,
    0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77, 0x7E,
    0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE, 0xE1, 0xE3, 0xE5, 0xE9, 0xF9};

std::string Hex8(unsigned v) { return std::format("${:02X}", v & 0xFF); }
std::string Hex16(unsigned v) { return std::format("${:04X}", v & 0xFFFF); }

// Reads operand bytes after the opcode.
struct Reader {
    std::span<const std::uint8_t> bytes;
    std::size_t at;  // the next byte
    bool ok = true;
    unsigned byte() {
        if (at >= bytes.size()) {
            ok = false;
            return 0;
        }
        return bytes[at++];
    }
    unsigned word() {
        const unsigned low = byte();
        return low | (byte() << 8);
    }
};

// The unprefixed instruction `op`, its operands read from `in`; empty if `op` is a prefix.
std::string Unprefixed(unsigned op, Reader& in, std::uint16_t address) {
    const unsigned x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    const auto relative = [&] {
        const auto e = static_cast<std::int8_t>(in.byte());
        return Hex16(static_cast<unsigned>(address + static_cast<int>(in.at) + e));
    };
    switch (x) {
    case 0:
        switch (z) {
        case 0:
            switch (y) {
            case 0:
                return "NOP";
            case 1:
                return "EX AF,AF'";
            case 2:
                return "DJNZ " + relative();
            case 3:
                return "JR " + relative();
            default:
                return std::format("JR {},", kCc[y - 4]) + relative();
            }
        case 1:
            if (q == 0) {
                return std::format("LD {},{}", kRp[p], Hex16(in.word()));
            }
            return std::format("ADD HL,{}", kRp[p]);
        case 2: {
            static constexpr std::array<std::string_view, 8> kForms{
                "LD (BC),A", "LD A,(BC)", "LD (DE),A", "LD A,(DE)",
                "LD (#),HL", "LD HL,(#)", "LD (#),A",  "LD A,(#)"};
            std::string text(kForms[p * 2 + q]);
            if (const auto hash = text.find('#'); hash != std::string::npos) {
                text.replace(hash, 1, Hex16(in.word()));
            }
            return text;
        }
        case 3:
            return std::format("{} {}", q == 0 ? "INC" : "DEC", kRp[p]);
        case 4:
            return std::format("INC {}", kR[y]);
        case 5:
            return std::format("DEC {}", kR[y]);
        case 6:
            return std::format("LD {},{}", kR[y], Hex8(in.byte()));
        default: {
            static constexpr std::array<std::string_view, 8> kOps{"RLCA", "RRCA", "RLA", "RRA",
                                                                  "DAA",  "CPL",  "SCF", "CCF"};
            return std::string(kOps[y]);
        }
        }
    case 1:
        if (y == 6 && z == 6) {
            return "HALT";
        }
        return std::format("LD {},{}", kR[y], kR[z]);
    case 2:
        return std::format("{}{}", kAlu[y], kR[z]);
    default:
        switch (z) {
        case 0:
            return std::format("RET {}", kCc[y]);
        case 1:
            if (q == 0) {
                return std::format("POP {}", kRp2[p]);
            }
            return std::array<std::string, 4>{"RET", "EXX", "JP (HL)", "LD SP,HL"}[p];
        case 2:
            return std::format("JP {},{}", kCc[y], Hex16(in.word()));
        case 3:
            switch (y) {
            case 0:
                return "JP " + Hex16(in.word());
            case 1:
                return "";  // CB
            case 2:
                return std::format("OUT ({}),A", Hex8(in.byte()));
            case 3:
                return std::format("IN A,({})", Hex8(in.byte()));
            case 4:
                return "EX (SP),HL";
            case 5:
                return "EX DE,HL";
            case 6:
                return "DI";
            default:
                return "EI";
            }
        case 4:
            return std::format("CALL {},{}", kCc[y], Hex16(in.word()));
        case 5:
            if (q == 0) {
                return std::format("PUSH {}", kRp2[p]);
            }
            return p == 0 ? "CALL " + Hex16(in.word()) : "";  // DD ED FD
        case 6:
            return std::format("{}{}", kAlu[y], Hex8(in.byte()));
        default:
            return "RST " + Hex8(y * 8);
        }
    }
}

std::optional<std::string> Cb(unsigned op, std::string_view m) {
    const unsigned x = op >> 6, y = (op >> 3) & 7;
    if (x == 0) {
        if (y == 6) {
            return std::nullopt;  // SLL
        }
        return std::format("{} {}", kRot[y], m);
    }
    static constexpr std::array<std::string_view, 4> kBit{"", "BIT", "RES", "SET"};
    return std::format("{} {},{}", kBit[x], y, m);
}

std::optional<std::string> Ed(unsigned op, Reader& in) {
    const unsigned x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    if (x == 2 && z <= 3 && y >= 4) {
        return std::string(kBlock[y - 4][z]);
    }
    if (x != 1) {
        return std::nullopt;
    }
    switch (z) {
    case 0:
        return y == 6 ? std::nullopt : std::optional<std::string>{std::format("IN {},(C)", kR[y])};
    case 1:
        return y == 6 ? std::nullopt : std::optional<std::string>{std::format("OUT (C),{}", kR[y])};
    case 2:
        return std::format("{} HL,{}", q == 0 ? "SBC" : "ADC", kRp[p]);
    case 3:
        if (p == 2) {
            return std::nullopt;  // ED 63, ED 6B
        }
        if (q == 0) {
            return std::format("LD ({}),{}", Hex16(in.word()), kRp[p]);
        }
        return std::format("LD {},({})", kRp[p], Hex16(in.word()));
    case 4:
        return y == 0 ? std::optional<std::string>{"NEG"} : std::nullopt;
    case 5:
        if (y > 1) {
            return std::nullopt;
        }
        return y == 0 ? "RETN" : "RETI";
    case 6:
        switch (y) {
        case 0:
            return "IM 0";
        case 2:
            return "IM 1";
        case 3:
            return "IM 2";
        default:
            return std::nullopt;
        }
    default: {
        static constexpr std::array<std::string_view, 6> kOps{"LD I,A", "LD R,A", "LD A,I",
                                                              "LD A,R", "RRD",    "RLD"};
        return y < 6 ? std::optional<std::string>{std::string(kOps[y])} : std::nullopt;
    }
    }
}

std::string Displacement(unsigned d, std::string_view index) {
    const auto e = static_cast<std::int8_t>(d);
    return e < 0 ? std::format("({}-{})", index, -e) : std::format("({}+{})", index, +e);
}

}  // namespace

std::optional<Disassembled> DisassembleZ80(std::span<const std::uint8_t> bytes, std::uint16_t at) {
    if (bytes.empty()) {
        return std::nullopt;
    }
    Reader in{bytes, 1};
    const unsigned first = bytes[0];
    std::optional<std::string> text;
    if (first == 0xCB) {
        const unsigned op = in.byte();
        text = Cb(op, kR[op & 7]);
    } else if (first == 0xED) {
        text = Ed(in.byte(), in);
    } else if (first == 0xDD || first == 0xFD) {
        const std::string_view index = first == 0xDD ? "IX" : "IY";
        const unsigned op = in.byte();
        if (op == 0xCB) {
            const unsigned d = in.byte();
            const unsigned cbOp = in.byte();
            if ((cbOp & 7) != 6) {
                return std::nullopt;
            }
            text = Cb(cbOp, Displacement(d, index));
        } else {
            bool documented = false;
            for (const std::uint8_t o : kIndexed) {
                documented = documented || o == op;
            }
            if (!documented) {
                return std::nullopt;
            }
            // (HL) becomes (IX+d), its d right after the opcode; then HL becomes IX.
            const bool memory = op == 0x34 || op == 0x35 || op == 0x36 ||
                                (op >= 0x40 && op < 0xC0 && ((op & 7) == 6 || (op & 0x38) == 0x30));
            std::string d;
            if (memory) {
                d = Displacement(in.byte(), index);
            }
            std::string plain = Unprefixed(op, in, at);
            if (memory) {
                plain.replace(plain.find("(HL)"), 4, d);
            } else {
                for (std::size_t i = plain.find("HL"); i != std::string::npos;
                     i = plain.find("HL", i)) {
                    plain.replace(i, 2, index);
                }
            }
            text = plain;
        }
    } else {
        text = Unprefixed(first, in, at);
    }
    if (!in.ok || !text || text->empty()) {
        return std::nullopt;
    }
    return Disassembled{*text, in.at};
}

}  // namespace bw
