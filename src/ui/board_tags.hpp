#pragma once

#include <vector>

#include "machine/machine.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"
#include "view/board_scene.hpp"

namespace bw {

// A Session's lamps as LedTags (their brightness) and its switches as SwitchTags (pressed as it
// reads them), on `scene` (its Machine's): the bench and the launcher's live run (P7b).
std::vector<LedTag> LedTagsOf(const BoardScene& scene, const Session& session);
std::vector<SwitchTag> SwitchTagsOf(const BoardScene& scene, const Session& session);
// A still Machine's (P7b): every lamp dark, every switch released, from its design Circuit.
std::vector<LedTag> StillLedTags(const BoardScene& scene, const LoadedCircuit& design);
std::vector<SwitchTag> StillSwitchTags(const BoardScene& scene, const LoadedCircuit& design);
// The Machine's cuts, cracks, Bodge Wires and Solder Bridges onto `view`.
void ShowRepairs(BoardView& view, const Machine& machine);

}  // namespace bw
