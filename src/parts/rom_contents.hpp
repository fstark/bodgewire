#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bw {

// The words of a ROM of `size` words of `width` bits (1-8), from a contents file's text: `;`
// comments, `@HHH` addresses, hex words (`HH`, `HH*N`), "strings", `.#` dot rows. Unwritten words
// are all ones (an erased EPROM). Errors: "<file> line N: <what>".
std::expected<std::vector<std::uint8_t>, std::string>
ParseRomContents(std::string_view text, std::string_view file, std::size_t size, int width);

// The same from a file; "cannot read '<path>'" if it cannot be opened. Errors name the file's
// filename only. A `.bin` file (any case) is a raw dump instead: one byte a word, exactly `size`
// bytes, `width` 8 (P5n: real ROM dumps, as the PET's in machines/130-bw2001/roms/).
std::expected<std::vector<std::uint8_t>, std::string>
ReadRomContents(const std::filesystem::path& path, std::size_t size, int width);

}  // namespace bw
