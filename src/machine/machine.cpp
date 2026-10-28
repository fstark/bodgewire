#include "machine/machine.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "board/board_check.hpp"
#include "board/copper.hpp"
#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "machine/electrical.hpp"

namespace bw {

namespace {

constexpr double kMendReach = 1'000'000;  // nm

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

std::expected<Machine, std::vector<Diagnostic>> LoadMachine(const Puzzle& puzzle,
                                                            const std::filesystem::path& root) {
    std::vector<Diagnostic> errors;
    const auto error = [&](std::uint32_t line, std::string message) {
        errors.push_back({puzzle.file, line, Severity::Error, std::move(message)});
    };
    const auto fail = [&](std::uint32_t line, std::string message) {
        error(line, std::move(message));
        return std::unexpected(std::move(errors));
    };

    Machine machine;
    const auto netlistPath = root / "netlists" / (puzzle.machine + ".bwn");
    const auto boardPath = root / "boards" / puzzle.machine / (puzzle.machine + ".kicad_pcb");
    const auto text = ReadFile(netlistPath);
    if (!text || !std::filesystem::exists(boardPath)) {
        return fail(puzzle.machineLine, "unknown machine '" + puzzle.machine + "'");
    }
    auto design = ParseNetlist(*text, netlistPath.filename().string());
    if (!design) {
        return std::unexpected(std::move(design.error()));
    }
    machine.design_ = std::move(*design);
    auto designCircuit = BuildCircuit(machine.design_);
    if (!designCircuit) {
        return std::unexpected(std::move(designCircuit.error()));
    }
    machine.designCircuit_ = std::move(*designCircuit);
    auto board = ReadKicadBoardFile(boardPath);
    if (!board) {
        return std::unexpected(std::move(board.error()));
    }
    machine.pristine_ = std::move(*board);
    auto problems = BoardNetlistProblems(*machine.designCircuit_);
    if (problems.empty()) {
        problems = CheckBoard(*machine.designCircuit_, machine.pristine_);
    }
    if (!problems.empty()) {
        return fail(puzzle.machineLine, "board does not match netlist: " + problems.front());
    }

    const LoadedCircuit& loaded = *machine.designCircuit_;
    for (const StuckAtDecl& decl : puzzle.stuck) {
        const auto component = loaded.components.find(decl.reference);
        if (component == loaded.components.end()) {
            error(decl.line, "unknown reference '" + decl.reference + "'");
            continue;
        }
        const auto pin = FindPin(*component->second, decl.pin);
        std::string part;
        for (const LoadedPart& p : loaded.parts) {
            if (p.reference == decl.reference) {
                part = p.info->name;
            }
        }
        if (!pin) {
            error(decl.line, decl.reference + " (" + part + ") has no pin " + decl.pin);
            continue;
        }
        const PinSpec& spec = component->second->pins()[*pin];
        if (!IsDriver(spec.kind)) {
            error(decl.line,
                  decl.reference + "." + decl.pin + " (" + spec.name + ") is not an output");
            continue;
        }
        machine.stuck_.push_back({decl.reference, *pin, decl.value});
    }

    const std::size_t islands = FindIslands(machine.pristine_).islandSize.size();
    for (const CrackDecl& crack : puzzle.cracks) {
        if (const auto trace = FindCrackTrace(machine.pristine_, crack.layer, crack.at); !trace) {
            error(crack.line, trace.error());
            continue;
        }
        if (FindIslands(CrackBoard(machine.pristine_, {&crack, 1})).islandSize.size() <= islands) {
            error(crack.line,
                  "crack at " + FormatPoint(crack.layer, crack.at) + " does not cut the copper");
            continue;
        }
        machine.cracks_.push_back(crack);
    }
    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    machine.board_ = CrackBoard(machine.pristine_, machine.cracks_);
    return machine;
}

bool Machine::swap(std::string_view reference) {
    if (!designCircuit_->components.contains(reference)) {
        return false;
    }
    std::erase_if(stuck_, [&](const StuckPin& s) { return s.reference == reference; });
    return true;
}

bool Machine::mend(Layer layer, Point at) {
    const auto removed = std::erase_if(cracks_, [&](const CrackDecl& crack) {
        return crack.layer == layer &&
               std::hypot(static_cast<double>(crack.at.x - at.x),
                          static_cast<double>(crack.at.y - at.y)) <= kMendReach;
    });
    if (removed == 0) {
        return false;
    }
    board_ = CrackBoard(pristine_, cracks_);
    return true;
}

LoadResult Machine::build() const {
    return BuildCircuit(ElectricalNetlist(design_, *designCircuit_, board_), stuck_);
}

std::expected<Machine, std::vector<Diagnostic>> LoadPuzzleFile(const std::filesystem::path& path) {
    std::string file = path.filename().string();
    const auto text = ReadFile(path);
    if (!text) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    const auto puzzle = ParsePuzzle(*text, std::move(file));
    if (!puzzle) {
        return std::unexpected(puzzle.error());
    }
    return LoadMachine(*puzzle, std::filesystem::absolute(path).parent_path().parent_path());
}

}  // namespace bw
