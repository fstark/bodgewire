#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "machine/puzzle.hpp"
#include "manual/manual.hpp"
#include "netlist/diagnostic.hpp"
#include "session/session.hpp"

namespace bw {

// A Machine (or a bare netlist) opened for play: from the command line, the launcher's live
// picture (P7b) or the bench it opens.
struct OpenedMachine {
    std::optional<Session> session;
    std::string title;  // Puzzle title, else Machine name, else netlist stem
    std::string brief;
    LoadedManuals manuals;   // problems printed as warnings, never fatal
    std::string fault = {};  // a Random fault's `fault …` line (P8d); "" otherwise
};

// A Random fault's post-it (P8d): its title, and its Brief's fixed sentence, before the Machine's
// task.
inline constexpr const char* kRandomFaultTitle = "Random fault";
inline constexpr const char* kRandomFaultBrief =
    "It worked, and now it does not. Nobody knows what happened to it.";

// The design Netlist's Circuit: references and pin numbers for manual links.
inline const LoadedCircuit& DesignOf(const Session& session) {
    return session.machine() ? session.machine()->designCircuit() : session.circuit();
}

// A netlist (with its Board when it has one) or a Puzzle, its manuals: diagnostics printed; no
// session on errors. Also from the launcher's background loads (P7b). A Machine's Vetting Script
// (puzzles/random.bwr) opens that Machine with `fault` injected, a Random fault's bench (P8d);
// without a Fault it is an error.
OpenedMachine Load(const std::filesystem::path& file,
                   const std::optional<std::filesystem::path>& roms,
                   const FaultDecl* fault = nullptr);

}  // namespace bw
