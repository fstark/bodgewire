#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "manual/markdown.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "parts/catalogue.hpp"

namespace bw {

struct Manuals {
    std::optional<Markdown> user, service;
    std::map<std::string, Markdown, std::less<>> datasheets;  // by Part name, Parts on the Machine
};

struct LoadedManuals {
    Manuals manuals;
    std::vector<Diagnostic> problems;  // Severity::Warning: unreadable or failing a check
};

// dir/manuals/{user,service}.md (`dir` the Machine's directory) and datasheets/<Part>.md for the
// design's Parts. Missing files are not problems.
LoadedManuals LoadManuals(const std::filesystem::path& dir, const std::filesystem::path& datasheets,
                          const LoadedCircuit& design);

// The shared datasheets/ of a Machine's directory (machines/NNN-name): the repository's.
std::filesystem::path DatasheetDir(const std::filesystem::path& machineDir);

// Each returns all problems found, empty if fine.
std::vector<Diagnostic> CheckUserManual(const Markdown& doc, std::string_view file,
                                        const LoadedCircuit& design);
std::vector<Diagnostic> CheckServiceManual(const Markdown& doc, std::string_view file,
                                           const LoadedCircuit& design);
std::vector<Diagnostic> CheckDatasheet(const Markdown& doc, std::string_view file,
                                       const PartInfo& part);

std::vector<std::string> PartPinNames(const PartInfo& part);  // empty without `create` (R)

// The Parts on a Machine, in netlist order of first use, each with its references in netlist
// order (P6e: the binder's datasheet index).
struct IndexEntry {
    std::string part;  // PartInfo::name: the key of Manuals::datasheets
    std::vector<std::string> references;
};
std::vector<IndexEntry> DatasheetIndex(const LoadedCircuit& design);

// "- `U6` end-of-frame decoder: detail" → {"end-of-frame decoder", "detail"}.
struct Role {
    std::string name, detail;
};
std::optional<Role> ComponentRole(const Markdown& service, std::string_view reference);

// Pin names a Datasheet `Pins` bullet covers: its code spans before the first ':', ranges
// `1A`–`1D` expanded.
std::vector<std::string> BulletPins(const Block& bullet);
const Block* PinBullet(const Markdown& datasheet, std::string_view pin);  // nullptr if none

// A backticked Component reference, optionally followed by " pin N", starting at spans[i].
struct ManualLink {
    std::string reference;
    std::optional<std::string> pin;  // number
    std::size_t pinChars = 0;  // leading chars of spans[i + 1] that are " pin N"; rest is text
};
std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design);

}  // namespace bw
