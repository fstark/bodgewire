#pragma once

#include <optional>
#include <string>

#include "session/session.hpp"

namespace bw {

// What bw_ui loaded from the command line.
struct Loaded {
    std::optional<Session> session;
    std::string title;  // Puzzle title, else Machine name, else netlist stem
    std::string brief;
};

}  // namespace bw
