#include "machine/machine_list.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#include "machine/machine.hpp"
#include "manual/markdown.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

namespace fs = std::filesystem;

// The entries of `dir` (no throw: an unreadable one lists nothing), sorted by name.
std::vector<fs::path> Entries(const fs::path& dir) {
    std::vector<fs::path> paths;
    std::error_code error;
    for (fs::directory_iterator it(dir, error), end; !error && it != end; it.increment(error)) {
        paths.push_back(it->path());
    }
    std::ranges::sort(paths);
    return paths;
}

bool IsDirectory(const fs::path& path) {
    std::error_code error;
    return fs::is_directory(path, error);
}

std::vector<fs::path> Netlists(const fs::path& dir) {
    std::vector<fs::path> netlists;
    for (const fs::path& path : Entries(dir)) {
        if (path.extension() == ".bwn" && !IsDirectory(path)) {
            netlists.push_back(path);
        }
    }
    return netlists;
}

bool HoldsMachine(const fs::path& machinesDir) {
    return std::ranges::any_of(Entries(machinesDir), [](const fs::path& path) {
        return IsDirectory(path) && !Netlists(path).empty();
    });
}

// The User Manual's title and first paragraph; empty without the file.
std::pair<std::string, std::string> NameAndLine(const fs::path& dir) {
    const auto text = ReadTextFile(dir / "manuals" / "user.md");
    if (!text) {
        return {};
    }
    const Markdown doc = ParseMarkdown(*text);
    std::string line;
    if (const auto paragraph = std::ranges::find(doc.blocks, BlockKind::Paragraph, &Block::kind);
        paragraph != doc.blocks.end()) {
        line = PlainText(paragraph->spans);
    }
    return {Title(doc), std::move(line)};
}

fs::path Canonical(const fs::path& path) {
    std::error_code error;
    fs::path canonical = fs::weakly_canonical(fs::absolute(path, error), error);
    return error ? path : canonical;
}

}  // namespace

MachineList ListMachines(const fs::path& machinesDir) {
    MachineList list;
    for (const fs::path& dir : Entries(machinesDir)) {
        if (!IsDirectory(dir)) {
            continue;
        }
        const std::vector<fs::path> netlists = Netlists(dir);
        if (netlists.empty()) {
            continue;
        }
        if (netlists.size() > 1) {
            list.problems.push_back({dir.filename().string(), 0, Severity::Warning,
                                     "more than one netlist: not a Machine"});
            continue;
        }
        MachineEntry machine{dir, netlists.front(), {}, {}, {}};
        auto [name, line] = NameAndLine(dir);
        machine.name = name.empty() ? dir.filename().string() : std::move(name);
        machine.line = std::move(line);
        const std::string stem = machine.netlist.stem().string();
        for (const fs::path& file : Entries(dir / "puzzles")) {
            if (file.extension() != ".bwp") {
                continue;
            }
            auto puzzle = ReadPuzzleFile(file);
            if (!puzzle) {
                list.problems.insert(list.problems.end(), puzzle.error().begin(),
                                     puzzle.error().end());
                continue;
            }
            if (puzzle->machine != stem) {
                list.problems.push_back({file.filename().string(), puzzle->machineLine,
                                         Severity::Warning,
                                         "machine '" + puzzle->machine + "' is not this " +
                                             "directory's '" + stem + "': left out"});
                continue;
            }
            machine.puzzles.push_back({file, std::move(puzzle->title), std::move(puzzle->brief)});
        }
        list.machines.push_back(std::move(machine));
    }
    return list;
}

std::optional<fs::path> FindMachinesDir(const fs::path& from) {
    fs::path dir = Canonical(from);
    while (true) {
        if (const fs::path machines = dir / "machines";
            IsDirectory(machines) && HoldsMachine(machines)) {
            return machines;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            return std::nullopt;
        }
        dir = dir.parent_path();
    }
}

std::optional<std::size_t> MachineIndexOf(const MachineList& list, const fs::path& file) {
    const fs::path dir = Canonical(MachineDir(file));
    for (std::size_t i = 0; i < list.machines.size(); ++i) {
        if (Canonical(list.machines[i].dir) == dir) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<EntryRef> EntryOf(const MachineList& list, const fs::path& file) {
    const fs::path wanted = Canonical(file);
    for (std::size_t m = 0; m < list.machines.size(); ++m) {
        const MachineEntry& machine = list.machines[m];
        if (Canonical(machine.netlist) == wanted) {
            return EntryRef{m, std::nullopt};
        }
        for (std::size_t p = 0; p < machine.puzzles.size(); ++p) {
            if (Canonical(machine.puzzles[p].file) == wanted) {
                return EntryRef{m, p};
            }
        }
    }
    return std::nullopt;
}

}  // namespace bw
