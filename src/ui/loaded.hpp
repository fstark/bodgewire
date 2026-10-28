#pragma once

#include <optional>
#include <string>

#include "manual/manual.hpp"
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

}  // namespace bw
