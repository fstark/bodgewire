#include "session/procedural_puzzle.hpp"

#include <utility>
#include <vector>

#include "machine/fault_draw.hpp"
#include "machine/vetting_script.hpp"
#include "netlist/diagnostic.hpp"

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

std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzleFor(const std::filesystem::path& script, std::uint64_t seed,
                        const std::optional<std::filesystem::path>& roms, std::stop_token stop) {
    const auto first = [](const std::vector<Diagnostic>& problems) {
        return problems.empty() ? std::string("cannot load") : Format(problems.front());
    };
    auto read = ReadVettingScriptFile(script);
    if (!read) {
        return std::unexpected(first(read.error()));
    }
    auto machine = LoadNetlistMachine(MachineDir(script) / (read->machine + ".bwn"), roms);
    if (!machine) {
        return std::unexpected(first(machine.error()));
    }
    auto vetting = Vetting::Make(std::move(*machine), std::move(*read), stop);
    if (!vetting) {
        return std::unexpected(std::move(vetting.error()));
    }
    if (!*vetting) {
        return std::optional<MadePuzzle>{};
    }
    Rng rng(seed);
    return MakeProceduralPuzzle(**vetting, rng, stop);
}

}  // namespace bw
