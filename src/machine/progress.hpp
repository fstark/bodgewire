#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "machine/machine_list.hpp"

namespace bw {

// The Puzzles the player has repaired (P8e), each by its key: its Machine's directory name and
// its PuzzleName, "030-dice/dead-button", so a Puzzle renumbered keeps its tick (renamed, it
// loses it).
class Progress {
public:
    bool has(std::string_view key) const { return keys_.contains(key); }
    bool add(std::string key) { return keys_.insert(std::move(key)).second; }  // true when new
    void merge(const Progress& other) { keys_.insert(other.keys_.begin(), other.keys_.end()); }
    void clear() { keys_.clear(); }
    bool empty() const { return keys_.empty(); }
    const std::set<std::string, std::less<>>& keys() const { return keys_; }
    // Parse met a key in the old form: the text it came from needs writing again.
    bool upgraded() const { return upgraded_; }

    // "# Bodgewire progress\n", then one key a line, sorted.
    std::string text() const;
    // Each line trimmed; blank and '#' lines skipped; duplicates once. A key in the form saved
    // until 2026-11-28, the path under machines/ ("030-dice/puzzles/01-dead-button.bwp"), is
    // read as today's ("030-dice/dead-button"), whatever its number. Never fails.
    static Progress Parse(std::string_view text);

private:
    std::set<std::string, std::less<>> keys_;
    bool upgraded_ = false;
};

// A listed Puzzle's key: its Machine's directory name, '/', its PuzzleName. No file system
// access: the Launcher calls it per post-it per frame.
std::string ProgressKey(const MachineEntry& machine, const PuzzleEntry& puzzle);
// The key of `file` when it is one of the list's Puzzles (EntryOf); nullopt for the working
// Machine, a Vetting Script, or a file not in the list.
std::optional<std::string> ProgressKey(const MachineList& list, const std::filesystem::path& file);

// Missing: empty. Present but not a regular file, or not opened: the error ("cannot read …").
std::expected<Progress, std::string> ReadProgressFile(const std::filesystem::path& path);
// Written to `path` + ".tmp", then renamed over it (a crash leaves the old file). The parent
// directory is not made. The error ("cannot write …").
std::expected<void, std::string> WriteProgressFile(const std::filesystem::path& path,
                                                   const Progress& progress);

}  // namespace bw
