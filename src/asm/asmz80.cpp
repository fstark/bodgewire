#include "asm/asmz80.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "text/text.hpp"

namespace bw::asm_detail {

namespace {

// --- operands -------------------------------------------------------------------------------

enum class Kind : std::uint8_t {
    Cond,     // NZ Z NC C PO PE P M (first operand of JP, CALL, JR, RET)
    Reg8,     // B C D E H L A: code 0-5, 7
    Special,  // I (code 0), R (code 1)
    Pair,     // BC DE HL SP AF: code 0-4
    Index,    // IX, IY
    AltAf,    // AF'
    AtHl,
    AtBc,
    AtDe,
    AtSp,
    AtC,
    AtIndex,  // (IX+e), (IX-e), (IX); expr empty for (IX)
    At,       // (expr): memory or a port
    Imm       // expr
};

struct Operand {
    Operand(Kind k = Kind::Imm, int c = 0, std::uint8_t p = 0, std::string_view e = {})
        : kind(k), code(c), prefix(p), expr(e) {}
    Kind kind = Kind::Imm;
    int code = 0;
    std::uint8_t prefix = 0;  // Index, AtIndex: $DD or $FD
    std::string_view expr;    // AtIndex (sign kept), At, Imm: from the original text
};

constexpr std::array<std::string_view, 8> kConditions{"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
constexpr std::array<std::string_view, 8> kRegisters{"B", "C", "D", "E", "H", "L", "", "A"};
constexpr std::array<std::string_view, 5> kPairs{"BC", "DE", "HL", "SP", "AF"};

template <std::size_t N>
std::optional<int> Find(const std::array<std::string_view, N>& names, std::string_view name) {
    for (std::size_t i = 0; i < N; ++i) {
        if (!names[i].empty() && names[i] == name) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

// True when `text` starts with ( and the ) matching it is its last character.
bool WhollyParenthesised(std::string_view text) {
    if (!text.starts_with('(')) {
        return false;
    }
    int depth = 0;
    bool inString = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inString) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
        } else if (c == '"') {
            inString = true;
        } else if (const std::size_t n = CharLiteralAt(text, i); n != 0) {
            i += n - 1;
        } else if (c == '(') {
            ++depth;
        } else if (c == ')') {
            if (--depth == 0) {
                return i + 1 == text.size();
            }
        }
    }
    return false;
}

Operand Parse(std::string_view text, bool conditionFirst) {
    const std::string_view original = Trim(text);
    const std::string s = Squeeze(original);
    if (conditionFirst) {
        if (const auto c = Find(kConditions, s)) {
            return {Kind::Cond, *c};
        }
    }
    if (const auto r = Find(kRegisters, s)) {
        return {Kind::Reg8, *r};
    }
    if (s == "I" || s == "R") {
        return {Kind::Special, s == "I" ? 0 : 1};
    }
    if (const auto p = Find(kPairs, s)) {
        return {Kind::Pair, *p};
    }
    if (s == "IX" || s == "IY") {
        return {Kind::Index, 0, static_cast<std::uint8_t>(s == "IX" ? 0xDD : 0xFD)};
    }
    if (s == "AF'") {
        return {Kind::AltAf};
    }
    if (s == "(HL)") {
        return {Kind::AtHl};
    }
    if (s == "(BC)") {
        return {Kind::AtBc};
    }
    if (s == "(DE)") {
        return {Kind::AtDe};
    }
    if (s == "(SP)") {
        return {Kind::AtSp};
    }
    if (s == "(C)") {
        return {Kind::AtC};
    }
    if (WhollyParenthesised(original)) {
        const std::string_view inner = Trim(original.substr(1, original.size() - 2));
        const std::string head = Upper(inner.substr(0, 2));
        if (head == "IX" || head == "IY") {
            const std::string_view rest = Trim(inner.substr(2));
            if (rest.empty() || rest.front() == '+' || rest.front() == '-') {
                Operand index{Kind::AtIndex, 0,
                              static_cast<std::uint8_t>(head == "IX" ? 0xDD : 0xFD)};
                index.expr = rest.starts_with('+') ? rest.substr(1) : rest;
                return index;
            }
        }
        return {Kind::At, 0, 0, original.substr(1, original.size() - 2)};
    }
    return {Kind::Imm, 0, 0, original};
}

// --- forms ----------------------------------------------------------------------------------

// Operand patterns of the form table (docs/p5t-z80-assembler/Z80_ASSEMBLER_DESIGN.md).
enum class P : std::uint8_t {
    None,
    M,      // r, (HL) or (XY+d): code 0-7, 6 for memory
    R,      // r only
    N,      // 8-bit data
    NN,     // 16-bit data
    Addr,   // JP, CALL target
    AtNN,   // (nn)
    Port,   // (n)
    DD,     // BC DE HL SP
    QQ,     // BC DE HL AF
    PP,     // BC DE XY SP, XY that of the first operand
    XY,     // IX, IY
    AtXY0,  // (IX), (IY): JP's
    CC,     // all 8 conditions
    JC,     // NZ Z NC C
    E,      // relative target
    Bit,    // 0-7
    Im,     // 0-2
    Rst,    // $00 $08 ... $38
    A,
    I,
    Rr,  // R
    HL,
    DE,
    AF,
    AFp,
    SP,
    AtBC,
    AtDE,
    AtSP,
    AtC,
    AtHL
};

struct Form {
    constexpr Form(std::string_view m, P pa, P pb, std::uint8_t pre, std::uint8_t op,
                   std::int8_t sa = -1, std::int8_t sb = -1)
        : mnemonic(m), a(pa), b(pb), prefix(pre), opcode(op), shiftA(sa), shiftB(sb) {}
    std::string_view mnemonic;
    P a = P::None, b = P::None;
    std::uint8_t prefix = 0;  // 0, $ED or $CB
    std::uint8_t opcode = 0;
    std::int8_t shiftA = -1, shiftB = -1;  // where each operand's code goes in the opcode
};

constexpr std::uint8_t ED = 0xED, CB = 0xCB;

// UM0080's documented instructions, tried in this order.
constexpr std::array kForms{
    // 8-bit load
    Form{"LD", P::M, P::M, 0, 0x40, 3, 0},
    Form{"LD", P::M, P::N, 0, 0x06, 3},
    Form{"LD", P::A, P::AtBC, 0, 0x0A},
    Form{"LD", P::A, P::AtDE, 0, 0x1A},
    Form{"LD", P::A, P::AtNN, 0, 0x3A},
    Form{"LD", P::AtBC, P::A, 0, 0x02},
    Form{"LD", P::AtDE, P::A, 0, 0x12},
    Form{"LD", P::AtNN, P::A, 0, 0x32},
    Form{"LD", P::A, P::I, ED, 0x57},
    Form{"LD", P::A, P::Rr, ED, 0x5F},
    Form{"LD", P::I, P::A, ED, 0x47},
    Form{"LD", P::Rr, P::A, ED, 0x4F},
    // 16-bit load
    Form{"LD", P::DD, P::NN, 0, 0x01, 4},
    Form{"LD", P::XY, P::NN, 0, 0x21},
    Form{"LD", P::HL, P::AtNN, 0, 0x2A},
    Form{"LD", P::DD, P::AtNN, ED, 0x4B, 4},
    Form{"LD", P::XY, P::AtNN, 0, 0x2A},
    Form{"LD", P::AtNN, P::HL, 0, 0x22},
    Form{"LD", P::AtNN, P::DD, ED, 0x43, -1, 4},
    Form{"LD", P::AtNN, P::XY, 0, 0x22},
    Form{"LD", P::SP, P::HL, 0, 0xF9},
    Form{"LD", P::SP, P::XY, 0, 0xF9},
    Form{"PUSH", P::QQ, P::None, 0, 0xC5, 4},
    Form{"PUSH", P::XY, P::None, 0, 0xE5},
    Form{"POP", P::QQ, P::None, 0, 0xC1, 4},
    Form{"POP", P::XY, P::None, 0, 0xE1},
    // exchange, block transfer and search
    Form{"EX", P::DE, P::HL, 0, 0xEB},
    Form{"EX", P::AF, P::AFp, 0, 0x08},
    Form{"EXX", P::None, P::None, 0, 0xD9},
    Form{"EX", P::AtSP, P::HL, 0, 0xE3},
    Form{"EX", P::AtSP, P::XY, 0, 0xE3},
    Form{"LDI", P::None, P::None, ED, 0xA0},
    Form{"LDIR", P::None, P::None, ED, 0xB0},
    Form{"LDD", P::None, P::None, ED, 0xA8},
    Form{"LDDR", P::None, P::None, ED, 0xB8},
    Form{"CPI", P::None, P::None, ED, 0xA1},
    Form{"CPIR", P::None, P::None, ED, 0xB1},
    Form{"CPD", P::None, P::None, ED, 0xA9},
    Form{"CPDR", P::None, P::None, ED, 0xB9},
    // 8-bit arithmetic and logic
    Form{"ADD", P::A, P::M, 0, 0x80, -1, 0},
    Form{"ADD", P::A, P::N, 0, 0xC6},
    Form{"ADC", P::A, P::M, 0, 0x88, -1, 0},
    Form{"ADC", P::A, P::N, 0, 0xCE},
    Form{"SUB", P::M, P::None, 0, 0x90, 0},
    Form{"SUB", P::N, P::None, 0, 0xD6},
    Form{"SBC", P::A, P::M, 0, 0x98, -1, 0},
    Form{"SBC", P::A, P::N, 0, 0xDE},
    Form{"AND", P::M, P::None, 0, 0xA0, 0},
    Form{"AND", P::N, P::None, 0, 0xE6},
    Form{"XOR", P::M, P::None, 0, 0xA8, 0},
    Form{"XOR", P::N, P::None, 0, 0xEE},
    Form{"OR", P::M, P::None, 0, 0xB0, 0},
    Form{"OR", P::N, P::None, 0, 0xF6},
    Form{"CP", P::M, P::None, 0, 0xB8, 0},
    Form{"CP", P::N, P::None, 0, 0xFE},
    Form{"INC", P::M, P::None, 0, 0x04, 3},
    Form{"DEC", P::M, P::None, 0, 0x05, 3},
    // general purpose
    Form{"DAA", P::None, P::None, 0, 0x27},
    Form{"CPL", P::None, P::None, 0, 0x2F},
    Form{"NEG", P::None, P::None, ED, 0x44},
    Form{"CCF", P::None, P::None, 0, 0x3F},
    Form{"SCF", P::None, P::None, 0, 0x37},
    Form{"NOP", P::None, P::None, 0, 0x00},
    Form{"HALT", P::None, P::None, 0, 0x76},
    Form{"DI", P::None, P::None, 0, 0xF3},
    Form{"EI", P::None, P::None, 0, 0xFB},
    Form{"IM", P::Im, P::None, ED, 0x46},
    // 16-bit arithmetic
    Form{"ADD", P::HL, P::DD, 0, 0x09, -1, 4},
    Form{"ADC", P::HL, P::DD, ED, 0x4A, -1, 4},
    Form{"SBC", P::HL, P::DD, ED, 0x42, -1, 4},
    Form{"ADD", P::XY, P::PP, 0, 0x09, -1, 4},
    Form{"INC", P::DD, P::None, 0, 0x03, 4},
    Form{"INC", P::XY, P::None, 0, 0x23},
    Form{"DEC", P::DD, P::None, 0, 0x0B, 4},
    Form{"DEC", P::XY, P::None, 0, 0x2B},
    // rotate and shift
    Form{"RLCA", P::None, P::None, 0, 0x07},
    Form{"RLA", P::None, P::None, 0, 0x17},
    Form{"RRCA", P::None, P::None, 0, 0x0F},
    Form{"RRA", P::None, P::None, 0, 0x1F},
    Form{"RLC", P::M, P::None, CB, 0x00, 0},
    Form{"RRC", P::M, P::None, CB, 0x08, 0},
    Form{"RL", P::M, P::None, CB, 0x10, 0},
    Form{"RR", P::M, P::None, CB, 0x18, 0},
    Form{"SLA", P::M, P::None, CB, 0x20, 0},
    Form{"SRA", P::M, P::None, CB, 0x28, 0},
    Form{"SRL", P::M, P::None, CB, 0x38, 0},
    Form{"RLD", P::None, P::None, ED, 0x6F},
    Form{"RRD", P::None, P::None, ED, 0x67},
    // bit set, reset and test
    Form{"BIT", P::Bit, P::M, CB, 0x40, 3, 0},
    Form{"SET", P::Bit, P::M, CB, 0xC0, 3, 0},
    Form{"RES", P::Bit, P::M, CB, 0x80, 3, 0},
    // jump
    Form{"JP", P::Addr, P::None, 0, 0xC3},
    Form{"JP", P::CC, P::Addr, 0, 0xC2, 3},
    Form{"JR", P::E, P::None, 0, 0x18},
    Form{"JR", P::JC, P::E, 0, 0x20, 3},
    Form{"JP", P::AtHL, P::None, 0, 0xE9},
    Form{"JP", P::AtXY0, P::None, 0, 0xE9},
    Form{"DJNZ", P::E, P::None, 0, 0x10},
    // call and return
    Form{"CALL", P::Addr, P::None, 0, 0xCD},
    Form{"CALL", P::CC, P::Addr, 0, 0xC4, 3},
    Form{"RET", P::None, P::None, 0, 0xC9},
    Form{"RET", P::CC, P::None, 0, 0xC0, 3},
    Form{"RETI", P::None, P::None, ED, 0x4D},
    Form{"RETN", P::None, P::None, ED, 0x45},
    Form{"RST", P::Rst, P::None, 0, 0xC7},
    // input and output
    Form{"IN", P::A, P::Port, 0, 0xDB},
    Form{"IN", P::R, P::AtC, ED, 0x40, 3},
    Form{"INI", P::None, P::None, ED, 0xA2},
    Form{"INIR", P::None, P::None, ED, 0xB2},
    Form{"IND", P::None, P::None, ED, 0xAA},
    Form{"INDR", P::None, P::None, ED, 0xBA},
    Form{"OUT", P::Port, P::A, 0, 0xD3},
    Form{"OUT", P::AtC, P::R, ED, 0x41, -1, 3},
    Form{"OUTI", P::None, P::None, ED, 0xA3},
    Form{"OTIR", P::None, P::None, ED, 0xB3},
    Form{"OUTD", P::None, P::None, ED, 0xAB},
    Form{"OTDR", P::None, P::None, ED, 0xBB},
};

bool TakesCondition(std::string_view mnemonic) {
    return mnemonic == "JP" || mnemonic == "CALL" || mnemonic == "JR" || mnemonic == "RET";
}

// One operand matched against a pattern: its code for the opcode, and what it adds.
struct Match {
    Match(int c = 0, bool m = false, std::uint8_t p = 0,
          std::optional<std::string_view> d = std::nullopt)
        : code(c), memory(m), prefix(p), displacement(d) {}
    int code = 0;
    bool memory = false;                           // M as (HL) or (XY+d)
    std::uint8_t prefix = 0;                       // $DD/$FD of an XY or (XY+d)
    std::optional<std::string_view> displacement;  // (XY+d): its text, empty for +0
};

std::optional<Match> MatchOperand(P pattern, const Operand& op, std::uint8_t prefixSoFar) {
    const auto literal = [&](bool ok) { return ok ? std::optional<Match>{Match{}} : std::nullopt; };
    switch (pattern) {
    case P::None:
        return std::nullopt;
    case P::M:
        if (op.kind == Kind::Reg8) {
            return Match{op.code};
        }
        if (op.kind == Kind::AtHl) {
            return Match{6, true};
        }
        if (op.kind == Kind::AtIndex) {
            return Match{6, true, op.prefix, op.expr};
        }
        return std::nullopt;
    case P::R:
        return op.kind == Kind::Reg8 ? std::optional<Match>{Match{op.code}} : std::nullopt;
    case P::N:
    case P::NN:
    case P::Addr:
    case P::E:
    case P::Bit:
    case P::Im:
    case P::Rst:
        return literal(op.kind == Kind::Imm);
    case P::AtNN:
    case P::Port:
        return literal(op.kind == Kind::At);
    case P::DD:
        return op.kind == Kind::Pair && op.code <= 3 ? std::optional<Match>{Match{op.code}}
                                                     : std::nullopt;
    case P::QQ:
        if (op.kind == Kind::Pair && op.code != 3) {
            return Match{op.code == 4 ? 3 : op.code};
        }
        return std::nullopt;
    case P::PP:
        if (op.kind == Kind::Pair && op.code != 2 && op.code != 4) {
            return Match{op.code};
        }
        if (op.kind == Kind::Index && op.prefix == prefixSoFar) {
            return Match{2};
        }
        return std::nullopt;
    case P::XY:
        if (op.kind == Kind::Index) {
            return Match{0, false, op.prefix};
        }
        return std::nullopt;
    case P::AtXY0:
        if (op.kind == Kind::AtIndex && op.expr.empty()) {
            return Match{0, false, op.prefix};
        }
        return std::nullopt;
    case P::CC:
        return op.kind == Kind::Cond ? std::optional<Match>{Match{op.code}} : std::nullopt;
    case P::JC:
        return op.kind == Kind::Cond && op.code < 4 ? std::optional<Match>{Match{op.code}}
                                                    : std::nullopt;
    case P::A:
        return literal(op.kind == Kind::Reg8 && op.code == 7);
    case P::I:
        return literal(op.kind == Kind::Special && op.code == 0);
    case P::Rr:
        return literal(op.kind == Kind::Special && op.code == 1);
    case P::HL:
        return literal(op.kind == Kind::Pair && op.code == 2);
    case P::DE:
        return literal(op.kind == Kind::Pair && op.code == 1);
    case P::AF:
        return literal(op.kind == Kind::Pair && op.code == 4);
    case P::AFp:
        return literal(op.kind == Kind::AltAf);
    case P::SP:
        return literal(op.kind == Kind::Pair && op.code == 3);
    case P::AtBC:
        return literal(op.kind == Kind::AtBc);
    case P::AtDE:
        return literal(op.kind == Kind::AtDe);
    case P::AtSP:
        return literal(op.kind == Kind::AtSp);
    case P::AtC:
        return literal(op.kind == Kind::AtC);
    case P::AtHL:
        return literal(op.kind == Kind::AtHl);
    }
    return std::nullopt;
}

class Z80Set final : public InstructionSet {
public:
    Z80Set() {
        for (const Form& form : kForms) {
            mnemonics_.emplace(form.mnemonic);
        }
    }

    bool knows(std::string_view mnemonic) const override {
        return mnemonics_.contains(std::string(mnemonic));
    }

    void assemble(AsmContext& context, std::string_view mnemonic,
                  std::string_view operandText) const override {
        std::vector<std::string_view> texts;
        if (!Trim(operandText).empty()) {
            texts = SplitArguments(operandText);
        }
        std::vector<Operand> ops;
        for (std::size_t i = 0; i < texts.size(); ++i) {
            ops.push_back(Parse(texts[i], i == 0 && TakesCondition(mnemonic)));
        }
        for (const Form& form : kForms) {
            if (form.mnemonic == mnemonic && tryForm(context, form, ops)) {
                return;
            }
        }
        std::string shown;
        for (std::size_t i = 0; i < texts.size(); ++i) {
            shown += (i == 0 ? "" : ",") + Squeeze(texts[i]);
        }
        std::string hint;
        if ((mnemonic == "ADD" || mnemonic == "ADC" || mnemonic == "SBC") && ops.size() == 1) {
            hint = std::format(" (Zilog: {} A,s)", mnemonic);
        } else if ((mnemonic == "SUB" || mnemonic == "AND" || mnemonic == "XOR" ||
                    mnemonic == "OR" || mnemonic == "CP") &&
                   ops.size() == 2 && ops[0].kind == Kind::Reg8 && ops[0].code == 7) {
            hint = std::format(" (Zilog: {} s)", mnemonic);
        }
        Fail(std::format("{} has no form '{}'{}", mnemonic, shown, hint));
    }

private:
    // Emits `form` if the operands match it.
    static bool tryForm(AsmContext& context, const Form& form, const std::vector<Operand>& ops) {
        const std::size_t count = form.a == P::None ? 0 : form.b == P::None ? 1 : 2;
        if (ops.size() != count) {
            return false;
        }
        std::array<Match, 2> matches{};
        std::uint8_t prefix = 0;
        const std::array<P, 2> patterns{form.a, form.b};
        for (std::size_t i = 0; i < count; ++i) {
            const auto m = MatchOperand(patterns[i], ops[i], prefix);
            if (!m) {
                return false;
            }
            if (m->prefix != 0) {
                if (prefix != 0 && prefix != m->prefix) {
                    return false;
                }
                prefix = m->prefix;
            }
            matches[i] = *m;
        }
        if (count == 2 && matches[0].memory && matches[1].memory) {
            return false;  // LD (HL),(HL) is HALT; no memory-to-memory load
        }
        emit(context, form, patterns, ops, matches, prefix);
        return true;
    }

    static void emit(AsmContext& context, const Form& form, const std::array<P, 2>& patterns,
                     const std::vector<Operand>& ops, const std::array<Match, 2>& matches,
                     std::uint8_t prefix) {
        const std::int64_t at = context.pc();
        int opcode = form.opcode;
        std::optional<AsmValue> displacement;
        std::vector<std::uint8_t> tail;  // operand bytes after the opcode
        const auto value = [&](std::size_t i) { return context.expression(ops[i].expr); };
        for (std::size_t i = 0; i < ops.size(); ++i) {
            const std::int8_t shift = i == 0 ? form.shiftA : form.shiftB;
            if (shift >= 0) {
                opcode += matches[i].code << shift;
            }
            if (matches[i].displacement) {
                displacement = matches[i].displacement->empty()
                                   ? AsmValue{0, true}
                                   : context.expression(*matches[i].displacement);
                context.check(*displacement, -128, 127, "displacement");
            }
            switch (patterns[i]) {
            case P::N: {
                const AsmValue v = value(i);
                context.check(v, -128, 255, "immediate");
                tail.push_back(static_cast<std::uint8_t>(v.number & 0xFF));
                break;
            }
            case P::NN:
            case P::Addr:
            case P::AtNN: {
                const AsmValue v = value(i);
                if (patterns[i] == P::NN) {
                    context.check(v, -32768, 0xFFFF, "immediate");
                } else {
                    context.check(v, 0, 0xFFFF, "address");
                }
                tail.push_back(static_cast<std::uint8_t>(v.number & 0xFF));
                tail.push_back(static_cast<std::uint8_t>((v.number >> 8) & 0xFF));
                break;
            }
            case P::Port: {
                const AsmValue v = value(i);
                context.check(v, 0, 255, "port");
                tail.push_back(static_cast<std::uint8_t>(v.number & 0xFF));
                break;
            }
            case P::E: {
                const AsmValue v = value(i);
                context.check(v, 0, 0xFFFF, "relative jump target");
                const std::int64_t offset = v.number - (at + 2);
                if (context.pass() == 2 && (offset < -128 || offset > 127)) {
                    Fail(std::format("relative jump out of range ({} bytes)", offset));
                }
                tail.push_back(static_cast<std::uint8_t>(offset & 0xFF));
                break;
            }
            case P::Bit: {
                const AsmValue v = value(i);
                context.check(v, 0, 7, "bit");
                opcode += static_cast<int>(v.number & 7) << 3;
                break;
            }
            case P::Im: {
                const AsmValue v = value(i);
                context.check(v, 0, 2, "interrupt mode");
                static constexpr std::array<std::uint8_t, 3> kModes{0x46, 0x56, 0x5E};
                opcode =
                    kModes[static_cast<std::size_t>(v.number >= 0 && v.number <= 2 ? v.number : 0)];
                break;
            }
            case P::Rst: {
                const AsmValue v = value(i);
                if (context.pass() == 2 && (v.number < 0 || v.number > 0x38 || v.number % 8 != 0)) {
                    Fail("RST needs $00, $08, ... $38");
                }
                opcode += static_cast<int>(v.number & 0x38);
                break;
            }
            default:
                break;
            }
        }
        if (prefix != 0) {
            context.emit(prefix);
        }
        if (form.prefix != 0) {
            context.emit(form.prefix);
        }
        const auto d = static_cast<std::uint8_t>(displacement ? displacement->number & 0xFF : 0);
        if (form.prefix == CB && displacement) {
            context.emit(d);  // DD CB d op
            context.emit(static_cast<std::uint8_t>(opcode));
        } else {
            context.emit(static_cast<std::uint8_t>(opcode));
            if (displacement) {
                context.emit(d);
            }
        }
        for (const std::uint8_t byte : tail) {
            context.emit(byte);
        }
    }

    std::set<std::string, std::less<>> mnemonics_;
};

}  // namespace

const InstructionSet& Z80Instructions() {
    static const Z80Set set;
    return set;
}

}  // namespace bw::asm_detail
