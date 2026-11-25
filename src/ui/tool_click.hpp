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

// The same named target (P8f, a finger's second tap): both swap one chip, resolder one pin, or
// join or part the same two pins. A mend and a cut name a point on copper: never the same by name.
bool SameRepair(const RepairAction& a, const RepairAction& b);
// What a plan would do, for a finger's first tap: "Swap U7", "Solder here", "Cut here", "Solder
// the wire".
std::string ActionName(const RepairPlan& plan);

// The Wire tool between its two clicks: the first pin and its pad.
struct WireDraft {
    std::optional<PinRef> start;
    Point at{};
};

// The Multimeter's black clip's copper (P8g): what a pick at the clip's point finds is `press`'s
// Hit (the pad, Trace or via it is on).
bool OnClip(const BoardScene& scene, const Pick& press, const TestLead& clip, PickReach reach);

// A click on the Board with the focus owner's tool (P6c, P6o). The probe, the instruments' leads
// and the pencil act at once; a tool that needs the power off returns its plan, which the caller
// applies, asks about or power-cycles (StepFor). A click the tool cannot take goes to `refuse`.
std::optional<RepairPlan> ToolClick(Owner owner, const BoardClick& click, Session& session,
                                    const BoardView& boardView, PencilMarks& marks, WireDraft& wire,
                                    const std::function<void(std::string)>& refuse);

}  // namespace bw
