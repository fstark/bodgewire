#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "session/vetting.hpp"
#include "sim/rng.hpp"

namespace bw {

// A Procedural Puzzle (P8d): one random Fault that shows.
struct MadePuzzle {
    FaultDecl fault;
    int tries;  // Faults vetted, this one included
};

inline constexpr int kTriesAKind = 5;  // Faults of one kind that do not show, then another kind
inline constexpr int kKindPicks = 60;  // then "no Fault shows on this Machine"

// Whether a Fault shows; nullopt: stopped. Vetting::shows, or a test's own.
using ShowsFn = std::function<std::optional<bool>(const FaultDecl&)>;

// A kind by weight (DrawKind), a Fault of it (DrawFault), vetted; another of the same kind while
// it does not show, kTriesAKind at most; then a kind again. `machine` is fault-free. nullopt:
// stopped. Error: after kKindPicks kinds, or on a Machine with nothing to put a Fault on.
std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzle(const Machine& machine, const ShowsFn& shows, Rng& rng);

// The same on a Vetting's Machine, vetted by it.
std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzle(Vetting& vetting, Rng& rng, std::stop_token stop = {});

}  // namespace bw
