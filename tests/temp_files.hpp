#pragma once

// Files written by tests into a temporary directory.

#include <filesystem>
#include <fstream>
#include <string_view>

namespace bw::test {

// `text` into `path`, its directories made.
inline void WriteFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

}  // namespace bw::test
