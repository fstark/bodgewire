#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "machine/repair.hpp"
#include "netlist/parser.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"
#include "view/focus.hpp"
#include "view/pencil_marks.hpp"

namespace bw {

// What a click with a tool that needs the power off would do (P6o): the repairs to try in order
// (the first Done wins: the iron resolders a pin, else mends), the puller's component (its pencil
// marks leave with it), the refusal when none is Done.
struct RepairPlan {
    std::vector<RepairAction> tries;
    std::optional<std::size_t> swapped;
    std::string unknown;
};

// The Wire tool between its two clicks: the first pin and its pad.
struct WireDraft {
    std::optional<PinRef> start;
    Point at{};
};

// A click on the Board with the focus owner's tool (P6c, P6o). The probe, the instruments' leads
// and the pencil act at once; a tool that needs the power off returns its plan, which the caller
// applies, asks about or power-cycles (StepFor). A click the tool cannot take goes to `refuse`.
std::optional<RepairPlan> ToolClick(Owner owner, const BoardClick& click, Session& session,
                                    const BoardView& boardView, PencilMarks& marks, WireDraft& wire,
                                    const std::function<void(std::string)>& refuse);

}  // namespace bw
