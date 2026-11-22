#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "netlist/parser.hpp"

namespace bw {

// The manifest a ROM directory may hold: which file fills which socket.
inline constexpr const char* kRomSocketsFile = "sockets.txt";

// ROMs chosen at run time (P5q2): replaces the contents= of each ROM Part (a Part with
// contents=). Two kinds of directory:
// - files named after the sockets (UH1.bin or UH1.rom), one chip each: each replaces its Part's
//   contents= and drops its offset=; other files are ignored;
// - a sockets.txt, which then alone says what goes where, a line a socket:
//   `UH5 bw2001_basic.rom offset=$800` (the file relative to the directory, offset= as the
//   netlist's, for one file over several chips); `#` starts a comment.
// Call after ResolveContentsPaths, before BuildCircuit. The directory is made absolute: the files
// are read again at every build. Errors: the directory missing; a REF.bin or REF.rom matching no
// ROM Part; two files for one Part; in sockets.txt, a line that is not `REF FILE [offset=N]`, a
// socket that is no ROM Part or comes twice, a file missing.
std::expected<void, std::string> ApplyRomDirectory(Netlist& netlist,
                                                   const std::filesystem::path& directory);

}  // namespace bw
