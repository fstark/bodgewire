#pragma once

#include <cstddef>
#include <cstdint>
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
// A Warning per Net of `design` reaching one pin that neither manual (null: none) names in a code
// span (P8j): a pin that goes nowhere has no Net name the player could meet but there.
std::vector<Diagnostic> CheckOnePinNets(const LoadedCircuit& design, const Markdown* user,
                                        const Markdown* service);

// The Parts on a Machine, in netlist order of first use, each with its references in netlist
// order (P6e: the binder's datasheet index).
struct IndexEntry {
    std::string part;  // PartInfo::name: the key of Manuals::datasheets
    std::vector<std::string> references;
};
std::vector<IndexEntry> DatasheetIndex(const LoadedCircuit& design);

// The Components bullet of `reference` after its leading code span ("- `U5` counter bits…"),
// the space after it dropped and its first letter upper-cased: a chip page's role sentence
// (P8j), its links drawn as the Service Manual's. nullopt if the bullet is absent.
std::optional<std::vector<Span>> ComponentRoleSpans(const Markdown& service,
                                                    std::string_view reference);

// The names a range `a`–`b` stands for, both included: equal-length names differing in one
// upper-case letter or digit (`1A`–`4A`, `A`–`D`), or one prefix before two numbers
// (`A0`–`A15`); {a, b} otherwise.
std::vector<std::string> ExpandRange(std::string_view a, std::string_view b);

// Pin names a Datasheet `Pins` bullet covers: its code spans before the first ':', ranges
// `1A`–`1D` expanded.
std::vector<std::string> BulletPins(const Block& bullet);
const Block* PinBullet(const Markdown& datasheet, std::string_view pin);  // nullptr if none

// Which links a page has: a chip's datasheet (shared by every Machine: its `CLK` or `D7` are pin
// names, never the LED D7) only that chip's pins and ranges of them (P8j); a Machine's own
// manual parts, runs, pins by name and Nets (P6i).
enum class LinkScope : std::uint8_t { Chip, Machine };

// A link starting at spans[i]: in a Machine's manual a part (" pin N" after it), a run
// `U1`–`U5`, a pin by name `U2.ENT`, or a Net `C19`; in a chip's sheet one of its pins or a range.
struct ManualLink {
    std::vector<std::string> references;  // one part, or a run's parts in netlist order
    std::optional<std::string> pin;       // a pin number, with one reference
    std::optional<std::string> net;       // a Net link: no references
    std::size_t spansUsed = 1;            // a run: 3 (`U1`, "–", `U5`)
    std::size_t pinChars = 0;       // leading chars of spans[i + 1] that are " pin N"; rest is text
    std::vector<std::string> pins;  // Chip scope: pin numbers, one or a range's; references {chip}
};
// `chip`: the reference whose pins the Chip scope links ("U5"); unused for Machine.
std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design, LinkScope scope,
                                 std::string_view chip = {});

}  // namespace bw
