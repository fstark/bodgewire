#include "ui/loaded.hpp"

#include <fstream>
#include <print>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "netlist/rom_directory.hpp"

namespace bw {

void Print(std::span<const Diagnostic> diagnostics) {
    for (const Diagnostic& d : diagnostics) {
        std::println(stderr, "{}", Format(d));
    }
}

Loaded Load(const std::filesystem::path& file, const std::optional<std::filesystem::path>& roms) {
    std::expected<Machine, std::vector<Diagnostic>> machine =
        std::unexpected(std::vector<Diagnostic>{});
    std::string title, brief;
    if (file.extension() == ".bwp") {
        const auto puzzle = ReadPuzzleFile(file);
        if (!puzzle) {
            Print(puzzle.error());
            return {};
        }
        title = puzzle->title;
        brief = puzzle->brief;
        machine = LoadMachine(*puzzle, MachineDir(file), roms);
    } else if (HasBoard(file)) {
        machine = LoadNetlistMachine(file, roms);
    } else {
        LoadResult result = LoadNetlistFile(file);
        if (roms) {
            std::ifstream in(file, std::ios::binary);
            std::stringstream text;
            text << in.rdbuf();
            auto netlist = ParseNetlist(text.str(), file.filename().string());
            if (!netlist) {
                Print(netlist.error());
                return {};
            }
            ResolveContentsPaths(*netlist, std::filesystem::absolute(file).parent_path());
            if (const auto applied = ApplyRomDirectory(*netlist, *roms); !applied) {
                std::println(stderr, "bw_ui: {}", applied.error());
                return {};
            }
            result = BuildCircuit(*netlist);
        }
        if (!result) {
            Print(result.error());
            return {};
        }
        Print((*result)->warnings);
        const auto dir = MachineDir(file);
        LoadedManuals manuals = LoadManuals(dir, DatasheetDir(dir), **result);
        Print(manuals.problems);
        return {Session(std::nullopt, std::move(*result)), file.stem().string(), "",
                std::move(manuals)};
    }
    if (!machine) {
        Print(machine.error());
        return {};
    }
    auto built = machine->build();
    if (!built) {
        Print(built.error());
        return {};
    }
    if (title.empty()) {
        title = machine->name();
    }
    const auto dir = MachineDir(file);
    LoadedManuals manuals = LoadManuals(dir, DatasheetDir(dir), machine->designCircuit());
    Print(manuals.problems);
    return {Session(std::move(*machine), std::move(*built)), std::move(title), std::move(brief),
            std::move(manuals)};
}

}  // namespace bw
