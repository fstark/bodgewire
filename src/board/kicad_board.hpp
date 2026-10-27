#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "netlist/diagnostic.hpp"

namespace bw {

constexpr std::int64_t kMinKicadBoardVersion = 20241229;  // KiCad 9

std::optional<std::int64_t> ParseMillimetres(std::string_view text);  // → nm, exact

using BoardResult = std::expected<Board, std::vector<Diagnostic>>;

BoardResult ReadKicadBoard(std::string_view text, std::string file);  // all errors, by line
BoardResult ReadKicadBoardFile(const std::filesystem::path& path);

}  // namespace bw
