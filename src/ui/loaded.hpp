#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "manual/manual.hpp"
#include "netlist/diagnostic.hpp"
#include "session/session.hpp"

namespace bw {

// What bw_ui loaded from the command line.
struct Loaded {
    std::optional<Session> session;
    std::string title;  // Puzzle title, else Machine name, else netlist stem
    std::string brief;
    LoadedManuals manuals;  // problems printed as warnings, never fatal
};

// The design Netlist's Circuit: references and pin numbers for manual links.
inline const LoadedCircuit& DesignOf(const Session& session) {
    return session.machine() ? session.machine()->designCircuit() : session.circuit();
}

void Print(std::span<const Diagnostic> diagnostics);  // each on stderr

// A netlist (with its Board when it has one) or a Puzzle, its manuals: diagnostics printed; no
// session on errors. Also from the launcher's background loads (P7b).
Loaded Load(const std::filesystem::path& file, const std::optional<std::filesystem::path>& roms);

}  // namespace bw
