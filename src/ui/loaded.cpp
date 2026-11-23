#include "ui/loaded.hpp"

#include <string>
#include <utility>
#include <vector>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"

namespace bw {

Loaded Load(const std::filesystem::path& file, const std::optional<std::filesystem::path>& roms) {
    std::expected<Machine, std::vector<Diagnostic>> machine =
        std::unexpected(std::vector<Diagnostic>{});
    std::string title, brief;
    if (file.extension() == ".bwp") {
        const auto puzzle = ReadPuzzleFile(file);
        if (!puzzle) {
            PrintDiagnostics(puzzle.error());
            return {};
        }
        title = puzzle->title;
        brief = puzzle->brief;
        machine = LoadMachine(*puzzle, MachineDir(file), roms);
    } else if (HasBoard(file)) {
        machine = LoadNetlistMachine(file, roms);
    } else {
        LoadResult result = LoadNetlistFile(file, roms);
        if (!result) {
            PrintDiagnostics(result.error());
            return {};
        }
        PrintDiagnostics((*result)->warnings);
        const auto dir = MachineDir(file);
        LoadedManuals manuals = LoadManuals(dir, DatasheetDir(dir), **result);
        PrintDiagnostics(manuals.problems);
        return {Session(std::nullopt, std::move(*result)), file.stem().string(), "",
                std::move(manuals)};
    }
    if (!machine) {
        PrintDiagnostics(machine.error());
        return {};
    }
    auto built = machine->build();
    if (!built) {
        PrintDiagnostics(built.error());
        return {};
    }
    if (title.empty()) {
        title = machine->name();
    }
    const auto dir = MachineDir(file);
    LoadedManuals manuals = LoadManuals(dir, DatasheetDir(dir), machine->designCircuit());
    PrintDiagnostics(manuals.problems);
    return {Session(std::move(*machine), std::move(*built)), std::move(title), std::move(brief),
            std::move(manuals)};
}

}  // namespace bw
