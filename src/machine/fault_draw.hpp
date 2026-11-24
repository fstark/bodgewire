#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "sim/rng.hpp"

namespace bw {

// The kinds of Fault a Procedural Puzzle may get (P8d): a .bwp's `fault` lines.
enum class FaultKind : std::uint8_t { Stuck, Crack, Bridge, Joint, Column, Cell, Switch };

// Shares of the kinds, in FaultKind's order (the P8d spec's starting weights).
inline constexpr std::array<int, 7> kFaultWeights{35, 32, 12, 10, 6, 3, 2};

inline constexpr int kDrawTries = 200;  // refused draws of one DrawFault before it gives up

// A kind by weight among those the Machine has something for (a memory chip for Column and Cell,
// a switch for Switch); nullopt for a Machine with no chip at all.
std::optional<FaultKind> DrawKind(const Machine& machine, Rng& rng);

// A random Fault of `kind` the Machine accepts (Machine::check), each of the Machine's objects of
// that kind as likely: a chip pin, two neighbouring pins, a pad of a Net with another pad of it
// and a place between them, a memory's data pin (and word), a switch. Refused draws (a power
// pin, the pins across the package, nothing crackable) are drawn again, kDrawTries at most;
// nullopt after that, or when the Machine has nothing of that kind.
std::optional<FaultDecl> DrawFault(const Machine& machine, FaultKind kind, Rng& rng);

}  // namespace bw
