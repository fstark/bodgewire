#pragma once

#include <cstdint>
#include <optional>

namespace bw {

// What a click with a tool that needs the power off does (P6o): apply its repair, ask first (the
// popup), or power-cycle the Machine around it at once.
enum class RepairStep : std::uint8_t { Apply, Ask, Cycle };

// Nothing when the click would apply no repair; with the power off, apply it; on, ask, or
// power-cycle at once once "Don't ask again" was ticked.
std::optional<RepairStep> StepFor(bool wouldRepair, bool powered, bool dontAsk);

}  // namespace bw
