#include "machine/machine.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "board/board_check.hpp"
#include "board/copper.hpp"
#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "machine/electrical.hpp"

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

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

std::expected<Machine, std::vector<Diagnostic>> LoadMachine(const Puzzle& puzzle,
                                                            const std::filesystem::path& root) {
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
    const auto netlistPath = root / "netlists" / (puzzle.machine + ".bwn");
    const auto boardPath = root / "boards" / puzzle.machine / (puzzle.machine + ".kicad_pcb");
    const auto text = ReadFile(netlistPath);
    if (!text || !std::filesystem::exists(boardPath)) {
        return fail(puzzle.machineLine, "unknown machine '" + puzzle.machine + "'");
    }
    auto design = ParseNetlist(*text, netlistPath.filename().string());
    if (!design) {
        return std::unexpected(std::move(design.error()));
    }
    ResolveContentsPaths(*design, netlistPath.parent_path());
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

    const LoadedCircuit& loaded = *machine.designCircuit_;
    const auto kindOf = [&](std::string_view reference) {
        const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
        return part == loaded.parts.end() ? PartKind::Resistor : part->info->kind;
    };
    for (const StuckAtDecl& decl : puzzle.stuck) {
        const auto component = loaded.components.find(decl.reference);
        if (component == loaded.components.end()) {
            error(decl.line, "unknown reference '" + decl.reference + "'");
            continue;
        }
        const auto pin = FindPin(*component->second, decl.pin);
        std::string part;
        for (const LoadedPart& p : loaded.parts) {
            if (p.reference == decl.reference) {
                part = p.info->name;
            }
        }
        if (!pin) {
            error(decl.line, decl.reference + " (" + part + ") has no pin " + decl.pin);
            continue;
        }
        const PinSpec& spec = component->second->pins()[*pin];
        const bool chipInput =
            kindOf(decl.reference) == PartKind::Chip && spec.kind == PinKind::Input;
        if (!IsDriver(spec.kind) && !chipInput) {
            error(decl.line, decl.reference + "." + decl.pin + " (" + spec.name +
                                 ") is not an output or a chip input");
            continue;
        }
        machine.stuck_.push_back({decl.reference, *pin, decl.value});
    }

    const auto known = [&](std::uint32_t line, const PinRef& pin) {
        const auto part = std::ranges::find(loaded.parts, pin.reference, &LoadedPart::reference);
        if (part == loaded.parts.end()) {
            error(line, "unknown reference '" + pin.reference + "'");
            return false;
        }
        if (!machine.padPosition(pin.reference, pin.pin)) {
            error(line,
                  pin.reference + " (" + std::string(part->info->name) + ") has no pin " + pin.pin);
            return false;
        }
        return true;
    };
    for (const CrackDecl& decl : puzzle.cracks) {
        if (!known(decl.line, decl.a) || !known(decl.line, decl.b)) {
            continue;
        }
        const auto crack = machine.placeCrack(decl.a, decl.b, decl.percent);
        if (!crack) {
            error(decl.line, crack.error());
            continue;
        }
        machine.cracks_.push_back(*crack);
    }
    for (const BridgeDecl& decl : puzzle.bridges) {
        const std::string written = "bridge " + decl.a.reference + "." + decl.a.pin + " " +
                                    decl.b.reference + "." + decl.b.pin;
        if (!known(decl.line, decl.a) || !known(decl.line, decl.b)) {
            continue;
        }
        if (!machine.padOf(decl.a) || !machine.padOf(decl.b)) {
            error(decl.line, written + ": pins of one chip only");
            continue;
        }
        const PinRef a = *machine.padOf(decl.a);
        const PinRef b = *machine.padOf(decl.b);
        if (a.reference != b.reference || a.pin == b.pin || kindOf(a.reference) != PartKind::Chip) {
            error(decl.line, written + ": pins of one chip only");
            continue;
        }
        const Point atA = *machine.padPosition(a.reference, a.pin);
        const Point atB = *machine.padPosition(b.reference, b.pin);
        if (std::hypot(static_cast<double>(atA.x - atB.x), static_cast<double>(atA.y - atB.y)) >
            kBridgeReach) {
            error(decl.line, written + ": pins are not neighbours");
            continue;
        }
        machine.bridges_.push_back({a, b, {atA, atB}});
    }
    for (const JointDecl& decl : puzzle.joints) {
        if (!known(decl.line, decl.pin)) {
            continue;
        }
        const auto pin = machine.chipSignalPin(decl.pin);
        if (!pin) {
            error(decl.line,
                  "joint " + decl.pin.reference + "." + decl.pin.pin + ": chip signal pins only");
            continue;
        }
        if (std::ranges::none_of(machine.dry_, [&](const PinRef& d) { return SamePin(d, *pin); })) {
            machine.dry_.push_back(*pin);
        }
    }
    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    machine.rebuildBoard();
    return machine;
}

bool Machine::swap(std::string_view reference) {
    if (!designCircuit_->components.contains(reference)) {
        return false;
    }
    std::erase_if(stuck_, [&](const StuckPin& s) { return s.reference == reference; });
    return true;
}

bool Machine::mend(Layer layer, Point at) {
    const auto near = [&](const Crack& crack) {
        return crack.layer == layer &&
               std::hypot(static_cast<double>(crack.at.x - at.x),
                          static_cast<double>(crack.at.y - at.y)) <= kMendReach;
    };
    const auto removed = std::erase_if(cracks_, near) + std::erase_if(cuts_, near);
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
    const auto it = FindEnds(bridges_, *pa, *pb);
    if (it == bridges_.end()) {
        return false;
    }
    bridges_.erase(it);
    rebuildBoard();
    return true;
}

bool Machine::resolder(const PinRef& pin) {
    const auto number = chipSignalPin(pin);
    if (!number) {
        return false;
    }
    std::erase_if(dry_, [&](const PinRef& d) { return SamePin(d, *number); });
    return true;
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
    std::vector<Crack> all = cracks_;
    all.insert(all.end(), cuts_.begin(), cuts_.end());
    board_ = CrackBoard(pristine_, all);
    for (const WireEnds& w : wires_) {
        board_.bodgeWires.push_back(w.wire);
    }
    for (const WireEnds& b : bridges_) {
        board_.bodgeWires.push_back(b.wire);
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
    for (const PinRef& dry : dry_) {
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
    return BuildCircuit(ElectricalNetlist(design_, *designCircuit_, board_, dry_), stuck_,
                        power_.unpowered);
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

std::filesystem::path MachineRoot(const std::filesystem::path& file) {
    return std::filesystem::absolute(file).parent_path().parent_path();
}

std::expected<Puzzle, std::vector<Diagnostic>> ReadPuzzleFile(const std::filesystem::path& path) {
    std::string file = path.filename().string();
    const auto text = ReadFile(path);
    if (!text) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    return ParsePuzzle(*text, std::move(file));
}

std::expected<Machine, std::vector<Diagnostic>> LoadPuzzleFile(const std::filesystem::path& path) {
    const auto puzzle = ReadPuzzleFile(path);
    if (!puzzle) {
        return std::unexpected(puzzle.error());
    }
    return LoadMachine(*puzzle, MachineRoot(path));
}

bool HasBoard(const std::filesystem::path& netlist) {
    const auto path = std::filesystem::absolute(netlist);
    const std::string stem = path.stem().string();
    const auto root = MachineRoot(path);
    return path.parent_path().filename() == "netlists" &&
           std::filesystem::exists(root / "boards" / stem / (stem + ".kicad_pcb"));
}

std::expected<Machine, std::vector<Diagnostic>>
LoadNetlistMachine(const std::filesystem::path& netlist) {
    const auto path = std::filesystem::absolute(netlist);
    return LoadMachine(Puzzle{.file = path.filename().string(), .machine = path.stem().string()},
                       MachineRoot(path));
}

}  // namespace bw
