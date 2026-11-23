#include "machine/machine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <utility>

#include "board/board_check.hpp"
#include "board/copper.hpp"
#include "board/geometry.hpp"
#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "machine/electrical.hpp"
#include "parts/memory_cells.hpp"
#include "parts/mos6530.hpp"
#include "parts/rom.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

constexpr double kMendReach = 1'000'000;    // nm
constexpr double kBridgeReach = 2'600'000;  // nm: neighbouring DIP pins are 2.54 mm apart

bool SamePin(const PinRef& x, const PinRef& y) {
    return x.reference == y.reference && x.pin == y.pin;
}

// The record joining pins `a` and `b` (numbers), either order.
std::vector<WireEnds>::iterator FindEnds(std::vector<WireEnds>& list, const PinRef& a,
                                         const PinRef& b) {
    return std::ranges::find_if(list, [&](const WireEnds& w) {
        return (SamePin(w.a, a) && SamePin(w.b, b)) || (SamePin(w.a, b) && SamePin(w.b, a));
    });
}

}  // namespace

std::expected<StuckCell, std::string> ResolveCell(const CellDecl& decl, const Component& component,
                                                  std::string_view part) {
    const auto* memory = dynamic_cast<const MemoryCells*>(&component);
    if (memory == nullptr) {
        return std::unexpected("RAM and ROM chips only");
    }
    const auto pin = FindPin(component, decl.pin.pin);
    if (!pin || !memory->dataBit(*pin)) {
        return std::unexpected("not a data pin");
    }
    const std::string chip = std::format("{} ({})", decl.pin.reference, part);
    const auto names = memory->memories();
    std::size_t index = 0;
    if (names.size() > 1 && decl.memory.empty()) {
        std::string list;
        for (std::size_t i = 0; i < names.size(); ++i) {
            list += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + std::string(names[i]);
        }
        return std::unexpected(chip + " has memories " + list + ": name one");
    }
    if (names.size() == 1 && !decl.memory.empty()) {
        return std::unexpected(chip + " has one memory: no name");
    }
    if (!decl.memory.empty()) {
        const auto it = std::ranges::find(names, decl.memory);
        if (it == names.end()) {
            return std::unexpected(chip + " has no memory '" + decl.memory + "'");
        }
        index = static_cast<std::size_t>(it - names.begin());
    }
    if (decl.word && *decl.word >= memory->wordCount(index)) {
        return std::unexpected(std::format("{}{} has {} words", chip,
                                           decl.memory.empty() ? "" : " " + decl.memory,
                                           memory->wordCount(index)));
    }
    return StuckCell{decl.pin.reference, *pin,
                     decl.word ? std::optional<std::size_t>(*decl.word) : std::nullopt, decl.value,
                     index};
}

std::expected<Machine, std::vector<Diagnostic>>
LoadMachine(const Puzzle& puzzle, const std::filesystem::path& dir,
            const std::optional<std::filesystem::path>& roms) {
    std::vector<Diagnostic> errors;
    const auto error = [&](std::uint32_t line, std::string message) {
        errors.push_back({puzzle.file, line, Severity::Error, std::move(message)});
    };
    const auto fail = [&](std::uint32_t line, std::string message) {
        error(line, std::move(message));
        return std::unexpected(std::move(errors));
    };

    Machine machine;
    machine.name_ = puzzle.machine;
    const auto netlistPath = dir / (puzzle.machine + ".bwn");
    const auto boardPath = dir / "board" / (puzzle.machine + ".kicad_pcb");
    if (!std::filesystem::is_regular_file(netlistPath) || !std::filesystem::exists(boardPath)) {
        return fail(puzzle.machineLine, "unknown machine '" + puzzle.machine + "'");
    }
    auto design = ReadNetlistFile(netlistPath, roms);
    if (!design) {
        return std::unexpected(std::move(design.error()));
    }
    machine.design_ = std::move(*design);
    auto designCircuit = BuildCircuit(machine.design_);
    if (!designCircuit) {
        return std::unexpected(std::move(designCircuit.error()));
    }
    machine.designCircuit_ = std::move(*designCircuit);
    auto board = ReadKicadBoardFile(boardPath);
    if (!board) {
        return std::unexpected(std::move(board.error()));
    }
    machine.pristine_ = std::move(*board);
    auto problems = BoardNetlistProblems(*machine.designCircuit_);
    if (problems.empty()) {
        problems = CheckBoard(*machine.designCircuit_, machine.pristine_);
    }
    if (!problems.empty()) {
        return fail(puzzle.machineLine, "board does not match netlist: " + problems.front());
    }

    machine.rebuildBoard();
    const auto add = [&](std::uint32_t line, const FaultDecl& fault) {
        if (auto added = machine.inject(fault); !added) {
            error(line, std::move(added.error()));
        }
    };
    for (const StuckAtDecl& decl : puzzle.stuck) {
        add(decl.line, decl);
    }
    for (const CrackDecl& decl : puzzle.cracks) {
        add(decl.line, decl);
    }
    for (const BridgeDecl& decl : puzzle.bridges) {
        add(decl.line, decl);
    }
    for (const JointDecl& decl : puzzle.joints) {
        add(decl.line, decl);
    }
    for (const CellDecl& decl : puzzle.cells) {
        add(decl.line, decl);
    }
    for (const SwitchDecl& decl : puzzle.switches) {
        add(decl.line, decl);
    }
    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    return machine;
}

std::expected<void, std::string> Machine::inject(const FaultDecl& fault) {
    using Result = std::expected<void, std::string>;
    const LoadedCircuit& loaded = *designCircuit_;
    const auto partOf = [&](std::string_view reference) -> const LoadedPart* {
        const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
        return part == loaded.parts.end() ? nullptr : &*part;
    };
    const auto kindOf = [&](std::string_view reference) {
        const LoadedPart* part = partOf(reference);
        return part == nullptr ? PartKind::Resistor : part->info->kind;
    };
    // A pin on the Board, or the error.
    const auto known = [&](const PinRef& pin) -> Result {
        const LoadedPart* part = partOf(pin.reference);
        if (part == nullptr) {
            return std::unexpected("unknown reference '" + pin.reference + "'");
        }
        if (!padPosition(pin.reference, pin.pin)) {
            return std::unexpected(pin.reference + " (" + std::string(part->info->name) +
                                   ") has no pin " + pin.pin);
        }
        return {};
    };
    const std::array<FaultSet*, 2> sets{&faults_, &loaded_};
    return std::visit(
        Overloaded{
            [&](const StuckAtDecl& decl) -> Result {
                const auto component = loaded.components.find(decl.reference);
                if (component == loaded.components.end()) {
                    return std::unexpected("unknown reference '" + decl.reference + "'");
                }
                const auto pin = FindPin(*component->second, decl.pin);
                if (!pin) {
                    const LoadedPart* part = partOf(decl.reference);
                    return std::unexpected(decl.reference + " (" +
                                           (part ? std::string(part->info->name) : "") +
                                           ") has no pin " + decl.pin);
                }
                const PinSpec& spec = component->second->pins()[*pin];
                const bool chipInput =
                    kindOf(decl.reference) == PartKind::Chip && spec.kind == PinKind::Input;
                if (!IsDriver(spec.kind) && !chipInput) {
                    return std::unexpected(decl.reference + "." + decl.pin + " (" + spec.name +
                                           ") is not an output or a chip input");
                }
                if (decl.input &&
                    (kindOf(decl.reference) != PartKind::Chip || !IsReader(spec.kind))) {
                    return std::unexpected(decl.reference + "." + decl.pin + " (" + spec.name +
                                           "): input needs a pin its chip reads");
                }
                for (FaultSet* set : sets) {
                    set->stuck.push_back({decl.reference, *pin, decl.value, decl.input});
                }
                return {};
            },
            [&](const CrackDecl& decl) -> Result {
                if (auto a = known(decl.a); !a) {
                    return a;
                }
                if (auto b = known(decl.b); !b) {
                    return b;
                }
                const auto crack = placeCrack(decl.a, decl.b, decl.percent);
                if (!crack) {
                    return std::unexpected(crack.error());
                }
                for (FaultSet* set : sets) {
                    set->cracks.push_back(*crack);
                }
                rebuildBoard();
                return {};
            },
            [&](const BridgeDecl& decl) -> Result {
                const std::string written = "bridge " + decl.a.reference + "." + decl.a.pin + " " +
                                            decl.b.reference + "." + decl.b.pin;
                if (auto a = known(decl.a); !a) {
                    return a;
                }
                if (auto b = known(decl.b); !b) {
                    return b;
                }
                const auto padA = padOf(decl.a);
                const auto padB = padOf(decl.b);
                if (!padA || !padB || padA->reference != padB->reference ||
                    padA->pin == padB->pin || kindOf(padA->reference) != PartKind::Chip) {
                    return std::unexpected(written + ": pins of one chip only");
                }
                const Point atA = *padPosition(padA->reference, padA->pin);
                const Point atB = *padPosition(padB->reference, padB->pin);
                if (Distance(atA, atB) > kBridgeReach) {
                    return std::unexpected(written + ": pins are not neighbours");
                }
                for (FaultSet* set : sets) {
                    set->bridges.push_back({*padA, *padB, {atA, atB}});
                }
                rebuildBoard();
                return {};
            },
            [&](const JointDecl& decl) -> Result {
                if (auto known_ = known(decl.pin); !known_) {
                    return known_;
                }
                const auto pin = chipSignalPin(decl.pin);
                if (!pin) {
                    return std::unexpected("joint " + decl.pin.reference + "." + decl.pin.pin +
                                           ": chip signal pins only");
                }
                for (FaultSet* set : sets) {
                    if (std::ranges::none_of(set->dry,
                                             [&](const PinRef& d) { return SamePin(d, *pin); })) {
                        set->dry.push_back(*pin);
                    }
                }
                return {};
            },
            [&](const CellDecl& decl) -> Result {
                const std::string pinText = decl.pin.reference + "." + decl.pin.pin;
                const std::string memory = decl.memory.empty() ? "" : " " + decl.memory;
                const std::string written =
                    decl.word ? std::format("cell {}{} ${:X}", pinText, memory, *decl.word)
                              : "column " + pinText + memory;
                if (auto known_ = known(decl.pin); !known_) {
                    return known_;
                }
                const auto component = loaded.components.find(decl.pin.reference);
                if (component == loaded.components.end()) {
                    return std::unexpected(written + ": RAM and ROM chips only");  // no Component
                }
                auto cell =
                    ResolveCell(decl, *component->second, partOf(decl.pin.reference)->info->name);
                if (!cell) {
                    return std::unexpected(written + ": " + cell.error());
                }
                for (FaultSet* set : sets) {
                    set->cells.push_back(*cell);
                }
                return {};
            },
            [&](const SwitchDecl& decl) -> Result {
                const LoadedPart* part = partOf(decl.reference);
                if (part == nullptr) {
                    return std::unexpected("unknown reference '" + decl.reference + "'");
                }
                if (part->info->kind != PartKind::Switch) {
                    return std::unexpected("switch " + decl.reference + ": buttons and keys only");
                }
                for (FaultSet* set : sets) {
                    std::erase_if(set->switches, [&](const StuckSwitch& s) {
                        return s.reference == decl.reference;
                    });
                    set->switches.push_back({decl.reference, decl.closed});
                }
                return {};
            },
        },
        fault);
}

bool Machine::swap(std::string_view reference) {
    if (!designCircuit_->components.contains(reference)) {
        return false;
    }
    std::erase_if(faults_.stuck, [&](const StuckPin& s) { return s.reference == reference; });
    std::erase_if(faults_.cells, [&](const StuckCell& c) { return c.reference == reference; });
    std::erase_if(faults_.switches, [&](const StuckSwitch& s) { return s.reference == reference; });
    return true;
}

std::optional<ChipContents> Machine::contents(std::string_view reference) const {
    const auto it = designCircuit_->components.find(reference);
    if (it == designCircuit_->components.end() ||
        (dynamic_cast<const Rom*>(it->second) == nullptr &&
         dynamic_cast<const Mos6530*>(it->second) == nullptr)) {
        return std::nullopt;
    }
    // The design build is never run: its cells are the netlist's, read on this thread.
    const auto& memory = dynamic_cast<const MemoryCells&>(*it->second);
    ChipContents contents{std::vector<std::uint8_t>(memory.wordCount(0)), memory.wordBits(0)};
    for (std::size_t w = 0; w < contents.words.size(); ++w) {
        contents.words[w] = memory.read(0, w);
    }
    // The current stuck cells and columns of its ROM (memory 0), as the builder sticks them.
    for (const StuckCell& cell : faults_.cells) {
        const auto bit = memory.dataBit(cell.pin);
        if (cell.reference != reference || cell.memory != 0 || !bit) {
            continue;
        }
        const auto mask = static_cast<std::uint8_t>(1u << *bit);
        const auto set = [&](std::uint8_t& word) {
            word = static_cast<std::uint8_t>(cell.value == LogicValue::One ? word | mask
                                                                           : word & ~mask);
        };
        if (cell.word) {
            set(contents.words[*cell.word]);
        } else {
            std::ranges::for_each(contents.words, set);
        }
    }
    return contents;
}

bool Machine::mend(Layer layer, Point at) {
    const auto near = [&](const Crack& crack) {
        return crack.layer == layer &&
               std::hypot(static_cast<double>(crack.at.x - at.x),
                          static_cast<double>(crack.at.y - at.y)) <= kMendReach;
    };
    const auto removed = std::erase_if(faults_.cracks, near) + std::erase_if(cuts_, near);
    if (removed == 0) {
        return false;
    }
    rebuildBoard();
    return true;
}

bool Machine::cut(Layer layer, Point at) {
    if (!FindCrackTrace(board_, layer, at)) {
        return false;
    }
    cuts_.push_back({layer, at});
    rebuildBoard();
    return true;
}

bool Machine::addWire(const PinRef& a, const PinRef& b) {
    const auto pa = padOf(a);
    const auto pb = padOf(b);
    if (!pa || !pb || (pa->reference == pb->reference && pa->pin == pb->pin)) {
        return false;
    }
    const auto atA = padPosition(pa->reference, pa->pin);
    const auto atB = padPosition(pb->reference, pb->pin);
    wires_.push_back({*pa, *pb, {*atA, *atB}});
    rebuildBoard();
    return true;
}

bool Machine::removeWire(const PinRef& a, const PinRef& b) {
    const auto pa = padOf(a);
    const auto pb = padOf(b);
    if (!pa || !pb) {
        return false;
    }
    const auto it = FindEnds(wires_, *pa, *pb);
    if (it == wires_.end()) {
        return false;
    }
    wires_.erase(it);
    rebuildBoard();
    return true;
}

bool Machine::removeBridge(const PinRef& a, const PinRef& b) {
    const auto pa = padOf(a);
    const auto pb = padOf(b);
    if (!pa || !pb) {
        return false;
    }
    const auto it = FindEnds(faults_.bridges, *pa, *pb);
    if (it == faults_.bridges.end()) {
        return false;
    }
    faults_.bridges.erase(it);
    rebuildBoard();
    return true;
}

bool Machine::apply(const RepairAction& repair) {
    return std::visit(Overloaded{[&](const SwapRepair& r) { return swap(r.reference); },
                                 [&](const MendRepair& r) { return mend(r.layer, r.at); },
                                 [&](const CutRepair& r) { return cut(r.layer, r.at); },
                                 [&](const WireRepair& r) { return addWire(r.a, r.b); },
                                 [&](const UnwireRepair& r) { return removeWire(r.a, r.b); },
                                 [&](const UnbridgeRepair& r) { return removeBridge(r.a, r.b); },
                                 [&](const ResolderRepair& r) { return resolder(r.pin); }},
                      repair);
}

bool Machine::resolder(const PinRef& pin) {
    const auto number = chipSignalPin(pin);
    if (!number) {
        return false;
    }
    std::erase_if(faults_.dry, [&](const PinRef& d) { return SamePin(d, *number); });
    return true;
}

void Machine::undoRepairs() {
    faults_ = loaded_;
    cuts_.clear();
    wires_.clear();
    rebuildBoard();
}

std::expected<Crack, std::string> Machine::placeCrack(const PinRef& a, const PinRef& b,
                                                      double percent) const {
    const std::string prefix =
        "crack " + a.reference + "." + a.pin + " " + b.reference + "." + b.pin + ": ";
    const auto atA = padPosition(a.reference, a.pin);
    const auto atB = padPosition(b.reference, b.pin);
    if (!atA || !atB) {
        return std::unexpected(prefix + "unknown pad");
    }
    if (*atA == *atB) {
        return std::unexpected(prefix + "two different pads");
    }
    auto crack = PlaceCrack(pristine_, *atA, *atB, percent);
    if (!crack) {
        return std::unexpected(prefix + crack.error());
    }
    return *crack;
}

void Machine::rebuildBoard() {
    std::vector<Crack> all = faults_.cracks;
    all.insert(all.end(), cuts_.begin(), cuts_.end());
    board_ = CrackBoard(pristine_, all);
    for (const WireEnds& w : wires_) {
        board_.padJoins.push_back(w.wire);
    }
    for (const WireEnds& b : faults_.bridges) {
        board_.padJoins.push_back(b.wire);
    }
    power_ = FindPower(*designCircuit_, board_, FindIslands(board_));
}

std::optional<PinRef> Machine::padOf(const PinRef& pin) const {
    const auto chip = designCircuit_->components.find(pin.reference);
    if (chip == designCircuit_->components.end()) {
        return std::nullopt;
    }
    const auto index = FindPin(*chip->second, pin.pin);
    if (!index) {
        return std::nullopt;
    }
    PinRef number{pin.reference, std::to_string(*index + 1)};
    if (!padPosition(number.reference, number.pin)) {
        return std::nullopt;
    }
    return number;
}

std::optional<PinRef> Machine::chipSignalPin(const PinRef& pin) const {
    const auto number = padOf(pin);
    if (!number) {
        return std::nullopt;
    }
    const auto part =
        std::ranges::find(designCircuit_->parts, number->reference, &LoadedPart::reference);
    if (part == designCircuit_->parts.end() || part->info->kind != PartKind::Chip) {
        return std::nullopt;
    }
    const PinKind kind =
        designCircuit_->components.at(number->reference)->pins()[std::stoul(number->pin) - 1].kind;
    if (kind == PinKind::Power || kind == PinKind::NoConnect) {
        return std::nullopt;
    }
    return number;
}

std::optional<PinRef> Machine::dryLegAt(Layer layer, Point at) const {
    if (layer != Layer::Front) {
        return std::nullopt;
    }
    for (const PinRef& dry : faults_.dry) {
        for (const BoardComponent& component : board_.components) {
            if (component.reference != dry.reference) {
                continue;
            }
            for (const Pad& pad : component.pads) {
                if (pad.number == dry.pin && Covers(pad, at)) {
                    return dry;
                }
            }
        }
    }
    return std::nullopt;
}

LoadResult Machine::build() const {
    return BuildCircuit(ElectricalNetlist(design_, *designCircuit_, board_, faults_.dry),
                        faults_.stuck, power_.unpowered, faults_.cells, faults_.switches);
}

std::optional<std::uint32_t> Machine::islandAt(const Islands& islands, Layer layer,
                                               Point at) const {
    std::optional<std::uint32_t> island;
    for (std::size_t k = 0; k < board_.components.size() && !island; ++k) {
        const auto& pads = board_.components[k].pads;
        for (std::size_t p = 0; p < pads.size() && !island; ++p) {
            if (Covers(pads[p], at)) {
                island = islands.padIsland[k][p];
            }
        }
    }
    for (std::size_t v = 0; v < board_.vias.size() && !island; ++v) {
        if (Covers(board_.vias[v], at)) {
            island = islands.viaIsland[v];
        }
    }
    for (std::size_t t = 0; t < board_.traces.size() && !island; ++t) {
        if (board_.traces[t].layer == layer && Covers(board_.traces[t], at)) {
            island = islands.traceIsland[t];
        }
    }
    return island;
}

bool Machine::connected(Layer layerA, Point a, Layer layerB, Point b) const {
    const auto legA = dryLegAt(layerA, a);
    const auto legB = dryLegAt(layerB, b);
    if (legA || legB) {
        return legA && legB && SamePin(*legA, *legB);
    }
    const Islands islands = FindIslands(board_);
    const auto first = islandAt(islands, layerA, a);
    return first && first == islandAt(islands, layerB, b);
}

std::optional<NetId> Machine::probeNet(const LoadedCircuit& built, Layer layer, Point at) const {
    if (const auto leg = dryLegAt(layer, at)) {
        return PinNet(built, leg->reference, leg->pin);
    }
    const Islands islands = FindIslands(board_);
    const auto island = islandAt(islands, layer, at);
    if (!island) {
        return std::nullopt;
    }
    const std::string name = IslandNetNames(*designCircuit_, board_, islands)[*island];
    if (name.empty()) {
        // No netlist Net reaches this island (an unconnected pin): its pad's own Net, as Clips
        // and Hooks read it.
        for (std::size_t c = 0; c < board_.components.size(); ++c) {
            const BoardComponent& component = board_.components[c];
            for (std::size_t p = 0; p < component.pads.size(); ++p) {
                if (islands.padIsland[c][p] == *island) {
                    return PinNet(built, component.reference, component.pads[p].number);
                }
            }
        }
        return std::nullopt;
    }
    const auto it = built.nets.find(name);
    return it == built.nets.end() ? std::nullopt : std::optional(it->second);
}

std::optional<Point> Machine::padPosition(std::string_view reference, std::string_view pin) const {
    for (const BoardComponent& component : pristine_.components) {
        if (component.reference != reference) {
            continue;
        }
        std::string number(pin);
        if (const auto chip = designCircuit_->components.find(reference);
            chip != designCircuit_->components.end()) {
            if (const auto index = FindPin(*chip->second, pin)) {
                number = std::to_string(*index + 1);
            }
        }
        for (const Pad& pad : component.pads) {
            if (pad.number == number) {
                return pad.position;
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::filesystem::path MachineDir(const std::filesystem::path& file) {
    const auto path = std::filesystem::absolute(file);
    return path.extension() == ".bwp" ? path.parent_path().parent_path() : path.parent_path();
}

std::expected<Puzzle, std::vector<Diagnostic>> ReadPuzzleFile(const std::filesystem::path& path) {
    std::string file = path.filename().string();
    const auto text = ReadTextFile(path);
    if (!text) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    return ParsePuzzle(*text, std::move(file));
}

std::expected<Machine, std::vector<Diagnostic>>
LoadPuzzleFile(const std::filesystem::path& path,
               const std::optional<std::filesystem::path>& roms) {
    const auto puzzle = ReadPuzzleFile(path);
    if (!puzzle) {
        return std::unexpected(puzzle.error());
    }
    return LoadMachine(*puzzle, MachineDir(path), roms);
}

bool HasBoard(const std::filesystem::path& netlist) {
    const auto path = std::filesystem::absolute(netlist);
    const std::string stem = path.stem().string();
    return std::filesystem::exists(path.parent_path() / "board" / (stem + ".kicad_pcb"));
}

std::expected<Machine, std::vector<Diagnostic>>
LoadNetlistMachine(const std::filesystem::path& netlist,
                   const std::optional<std::filesystem::path>& roms) {
    const auto path = std::filesystem::absolute(netlist);
    return LoadMachine(Puzzle{.file = path.filename().string(), .machine = path.stem().string()},
                       MachineDir(path), roms);
}

}  // namespace bw
