#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "board/board.hpp"
#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "sim/logic.hpp"

namespace bw {

// Syntax tree of a .bwp file; no Machine knowledge.
struct StuckAtDecl {
    std::uint32_t line;
    std::string reference, pin;
    LogicValue value;    // Zero or One
    bool input = false;  // `input`: the chip reads the value, its Net left alone (a port pin)
};

struct CrackDecl {
    std::uint32_t line;
    PinRef a, b;     // pin by number or name: U1.QA, R6.2
    double percent;  // 0–100 along the crackable Copper from a to b
};

struct BridgeDecl {
    std::uint32_t line;
    PinRef a, b;  // pin by number or name: U1.7, U1.ENP
};

struct JointDecl {
    std::uint32_t line;
    PinRef pin;  // by number or name: U15.13, U15.2D
};

struct CellDecl {
    std::uint32_t line;
    PinRef pin;                         // the data pin, by number or name: U3.IO2, U3.11
    std::optional<std::uint32_t> word;  // the chip's own address; nullopt: `fault column`
    LogicValue value;                   // Zero or One
    std::string memory = {};            // "rom" or "ram" on a 6530; "" when not written
};

struct SwitchDecl {
    std::uint32_t line;
    std::string reference;  // a BUTTON or KEY
    bool closed;            // `closed`: stuck closed; `open`: never closes
};

// Any one Fault as written: a Puzzle's `fault` line, or Machine::inject's.
using FaultDecl = std::variant<StuckAtDecl, CrackDecl, BridgeDecl, JointDecl, CellDecl, SwitchDecl>;
std::uint32_t LineOf(const FaultDecl& fault);

struct Puzzle {
    std::string file;
    std::uint32_t machineLine = 0;
    std::string machine;       // "test_pattern": [a-z0-9_]+, so it is safe in a path
    std::string title, brief;  // "" when absent
    std::uint32_t titleLine = 0, briefLine = 0;
    std::vector<FaultDecl> faults;  // in file order
};

// A Machine's netlist name as a .bwp and a .bwr write it: [a-z0-9_]+.
bool IsMachineName(std::string_view text);
// `"TEXT"` then only spaces or a # comment: TEXT (a Puzzle's title and brief, a Vetting Script's
// task); nullopt otherwise.
std::optional<std::string> QuotedValue(std::string_view rest);

// All syntax errors, in line order.
std::expected<Puzzle, std::vector<Diagnostic>> ParsePuzzle(std::string_view text, std::string file);

}  // namespace bw
