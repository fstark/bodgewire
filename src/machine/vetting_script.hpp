#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "machine/knob_setting.hpp"
#include "machine/press.hpp"
#include "machine/typing.hpp"
#include "netlist/diagnostic.hpp"
#include "sim/time.hpp"

namespace bw {

// A Machine's Vetting Script, puzzles/random.bwr (P8d): how a Procedural Puzzle is vetted on it
// (the inputs played from power-on, the capture times its outputs are looked at) and what its
// Brief asks of the player. No Machine knowledge: references are checked by InputEdges.
struct VettingScript {
    std::string file;
    std::uint32_t machineLine = 0;
    std::string machine;  // "dice": [a-z0-9_]+
    std::uint32_t taskLine = 0;
    std::string task;  // "" when absent
    std::vector<ButtonPress> presses;
    std::vector<KnobSetting> knobs;
    std::vector<Typing> typing;
    std::vector<Time> captures;  // capture times, ps, ascending, at least one
};

// Lines: `machine NAME`, `task "TEXT"`, `press REF:DOWN:UP`, `knob REF:N%[:TIME]`,
// `type REF:START:TEXT` (the rest of the line), `capture TIME`; # starts a comment, but not on a
// task or a type line. All syntax errors, in line order.
std::expected<VettingScript, std::vector<Diagnostic>> ParseVettingScript(std::string_view text,
                                                                         std::string file);

// Read + ParseVettingScript.
std::expected<VettingScript, std::vector<Diagnostic>>
ReadVettingScriptFile(const std::filesystem::path& path);

}  // namespace bw
