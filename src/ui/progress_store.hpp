#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "machine/progress.hpp"
#include "view/bench_options.hpp"

namespace bw {

// Where the player's Progress lives (P8e): a file (--progress, else progress.txt in
// SDL_GetPrefPath("bodgewire", "bw_ui")); on the web always localStorage["bodgewire.progress"];
// or nowhere (--screenshot without --progress: nothing read, nothing written; and after a load
// that failed, so a file that could not be read is never written over).
class ProgressStore {
public:
    static ProgressStore For(const Options& options);  // after SDL_Init
    // An error on stderr, then empty, and the store keeps nothing from then on.
    Progress load();
    // Re-read, merged with `progress` and `key` added, written: `progress` becomes the merge
    // (another instance's ticks kept). Errors on stderr.
    void add(Progress& progress, std::string key);
    void clear(Progress& progress);  // `progress` emptied and written empty

private:
    enum class Where : std::uint8_t { None, File, Browser };
    void write(const Progress& progress);

    Where where_ = Where::None;
    std::filesystem::path file_;
};

}  // namespace bw
