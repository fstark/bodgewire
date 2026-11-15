#pragma once

// Internal to bw_asm: what the front end (assembler.cpp) and each CPU's instructions share
// (docs/p5t-z80-assembler/Z80_ASSEMBLER_DESIGN.md, Front end).

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bw::asm_detail {

// An assembly error; the front end adds the file and line.
struct AsmError {
    std::string message;
};

[[noreturn]] inline void Fail(std::string message) { throw AsmError{std::move(message)}; }

// A value from an expression: unknown while it names a symbol not yet defined (pass 1).
struct AsmValue {
    std::int64_t number = 0;
    bool known = true;
};

std::string Upper(std::string_view text);
std::string_view Trim(std::string_view text);
bool IsIdentStart(char c);
bool IsIdentChar(char c);
// The length of a 'c' literal starting at text[i], or 0.
std::size_t CharLiteralAt(std::string_view text, std::size_t i);
// Splits on commas outside "text", 'c' and parentheses.
std::vector<std::string_view> SplitArguments(std::string_view text);
// The text without blanks outside 'c' literals, upper case outside them.
std::string Squeeze(std::string_view text);

// The front end, as an instruction set sees it.
class AsmContext {
public:
    virtual int pass() const = 0;         // 1 or 2
    virtual std::int64_t pc() const = 0;  // fails before any .org
    virtual AsmValue expression(std::string_view text) = 0;
    virtual void emit(std::uint8_t byte) = 0;
    // In pass 2 only: fails with "<what> value N out of range" outside low..high.
    virtual void check(AsmValue v, std::int64_t low, std::int64_t high, std::string_view what) = 0;
    virtual int& memo() = 0;  // one int per source line, kept from pass 1 to pass 2

protected:
    ~AsmContext() = default;
};

// One CPU's instructions.
class InstructionSet {
public:
    virtual bool knows(std::string_view upperMnemonic) const = 0;
    virtual void assemble(AsmContext& context, std::string_view upperMnemonic,
                          std::string_view operands) const = 0;

protected:
    ~InstructionSet() = default;
};

}  // namespace bw::asm_detail
