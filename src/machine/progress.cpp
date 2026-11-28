#include "machine/progress.hpp"

#include <fstream>
#include <system_error>

#include "text/text.hpp"

namespace bw {

namespace {

constexpr std::string_view kHeader = "# Bodgewire progress\n";
constexpr std::string_view kOldPuzzles = "/puzzles/";

std::string Key(std::string_view machineDir, const std::filesystem::path& file) {
    return std::string(machineDir) + '/' + PuzzleName(file);
}

// "030-dice/puzzles/01-dead-button.bwp" (until 2026-11-28) → "030-dice/dead-button".
std::string Upgraded(std::string_view key) {
    const auto at = key.find(kOldPuzzles);
    if (at == std::string_view::npos || !key.ends_with(".bwp")) {
        return std::string(key);
    }
    return Key(key.substr(0, at), std::filesystem::path(key.substr(at + kOldPuzzles.size())));
}

}  // namespace

std::string Progress::text() const {
    std::string text(kHeader);
    for (const std::string& key : keys_) {
        text += key;
        text += '\n';
    }
    return text;
}

Progress Progress::Parse(std::string_view text) {
    Progress progress;
    for (const std::string_view line : Split(text, '\n')) {
        const std::string_view key = Trim(line);
        if (!key.empty() && !key.starts_with('#')) {
            progress.add(Upgraded(key));
        }
    }
    return progress;
}

std::string ProgressKey(const MachineEntry& machine, const PuzzleEntry& puzzle) {
    return Key(machine.dir.filename().generic_string(), puzzle.file);
}

std::optional<std::string> ProgressKey(const MachineList& list, const std::filesystem::path& file) {
    const auto entry = EntryOf(list, file);
    if (!entry || !entry->puzzle || entry->random) {
        return std::nullopt;
    }
    const MachineEntry& machine = list.machines[entry->machine];
    return ProgressKey(machine, machine.puzzles[*entry->puzzle]);
}

std::expected<Progress, std::string> ReadProgressFile(const std::filesystem::path& path) {
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    if (status.type() == std::filesystem::file_type::not_found) {
        return Progress{};
    }
    const auto error = std::unexpected("cannot read '" + path.string() + "'");
    if (ec || status.type() != std::filesystem::file_type::regular) {
        return error;
    }
    const auto text = ReadTextFile(path);
    if (!text) {
        return error;
    }
    return Progress::Parse(*text);
}

std::expected<void, std::string> WriteProgressFile(const std::filesystem::path& path,
                                                   const Progress& progress) {
    const auto error = std::unexpected("cannot write '" + path.string() + "'");
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            return error;
        }
        out << progress.text();
        out.close();
        if (!out) {
            return error;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return error;
    }
    return {};
}

}  // namespace bw
