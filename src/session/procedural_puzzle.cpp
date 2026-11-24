#include "session/procedural_puzzle.hpp"

#include "machine/fault_draw.hpp"

namespace bw {

std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzle(const Machine& machine, const ShowsFn& shows, Rng& rng) {
    int tries = 0;
    for (int pick = 0; pick < kKindPicks; ++pick) {
        const auto kind = DrawKind(machine, rng);
        if (!kind) {
            return std::unexpected("this Machine has nothing to put a Fault on");
        }
        for (int same = 0; same < kTriesAKind; ++same) {
            const auto fault = DrawFault(machine, *kind, rng);
            if (!fault) {
                break;  // nothing of this kind the Machine accepts: another kind
            }
            ++tries;
            const std::optional<bool> shown = shows(*fault);
            if (!shown) {
                return std::optional<MadePuzzle>{};
            }
            if (*shown) {
                return std::optional(MadePuzzle{*fault, tries});
            }
        }
    }
    return std::unexpected("no Fault shows on this Machine");
}

std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzle(Vetting& vetting, Rng& rng, std::stop_token stop) {
    return MakeProceduralPuzzle(
        vetting.machine(), [&](const FaultDecl& fault) { return vetting.shows(fault, stop); }, rng);
}

}  // namespace bw
