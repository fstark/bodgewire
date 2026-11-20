#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "board/copper.hpp"
#include "machine/crack.hpp"
#include "machine/electrical.hpp"
#include "machine/puzzle.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"

namespace bw {

class Machine;

// A ROM chip's words (P6p).
struct ChipContents {
    std::vector<std::uint8_t> words;
    unsigned bits = 8;  // per word
};

// A Bodge Wire with its ends as pin numbers.
struct WireEnds {
    PinRef a, b;
    BodgeWire wire;  // pad positions
};

// dir/<machine>.bwn + dir/board/<machine>.kicad_pcb, `dir` the Machine's directory
// (machines/NNN-name); all semantic errors.
std::expected<Machine, std::vector<Diagnostic>>
LoadMachine(const Puzzle& puzzle, const std::filesystem::path& dir,
            const std::optional<std::filesystem::path>& roms = std::nullopt);

// Design netlist + Board + Faults. Repairs edit it; build() makes the Circuit for one power-on.
class Machine {
public:
    std::string_view name() const {
        return name_;
    }  // never the Puzzle's file name: it names the Fault
    // Chip or switch swap: removes its stuck pins, cells, columns and switch Fault; false if no
    // such Component.
    bool swap(std::string_view reference);
    bool mend(Layer layer, Point at);  // removes cracks and Cuts within 1 mm
    bool cut(Layer layer, Point at);   // false: no straight Trace there in the current Copper
    bool addWire(const PinRef& a, const PinRef& b);       // false: unknown pad, or the same pad
    bool removeWire(const PinRef& a, const PinRef& b);    // either order; false: no such wire
    bool removeBridge(const PinRef& a, const PinRef& b);  // either order; false: no such bridge
    // Resolders a chip pin's Joint (number or name): removes its dry Joint if any. True also on a
    // good Joint (the tool must not tell); false: unknown, or not a chip signal pin.
    bool resolder(const PinRef& pin);
    std::span<const PinRef> dryJoints() const { return dry_; }  // pin numbers
    std::span<const Crack> cracks() const { return cracks_; }   // the Puzzle's Broken Traces
    // A crack on the fault-free Board; errors say "crack A B: …" with the pins as given.
    std::expected<Crack, std::string> placeCrack(const PinRef& a, const PinRef& b,
                                                 double percent) const;
    std::span<const Crack> cuts() const { return cuts_; }
    std::span<const WireEnds> wires() const { return wires_; }
    std::span<const WireEnds> bridges() const { return bridges_; }
    LoadResult build() const;                      // electrical Circuit with the current Faults
    const Board& board() const { return board_; }  // with cracks, Cuts and Bodge Wires
    const Board& pristineBoard() const { return pristine_; }  // fault-free Copper, for the view
    const LoadedCircuit& designCircuit() const { return *designCircuit_; }
    const BoardPower& power() const { return power_; }  // of the current Copper
    // A ROM chip's words as it holds them now (P6p): the design's, through the current stuck
    // cells and columns of its ROM (a swap clears them at once, before the next build). nullopt:
    // no such chip, or not a ROM (a RAM's cells belong to the running Machine).
    std::optional<ChipContents> contents(std::string_view reference) const;

    // Electrical Net under `at` in `built` (this Machine's build()), in the current Copper: a pad
    // or via on either Layer, else a Trace on `layer`. nullopt off Copper or on an unnamed island.
    // A dry Joint's pad with F.Cu active reads the leg: the pin's own Net.
    std::optional<NetId> probeNet(const LoadedCircuit& built, Layer layer, Point at) const;
    // Position of a Pin's pad: "U1", "QA" or "14"; nullopt if unknown.
    std::optional<Point> padPosition(std::string_view reference, std::string_view pin) const;
    // Same copper island under both points, in the current Copper (continuity). A dry leg (F.Cu
    // on its pad) is joined to nothing but itself.
    bool connected(Layer layerA, Point a, Layer layerB, Point b) const;

private:
    Machine() = default;
    // Island of the current Copper under `at`; nullopt off Copper.
    std::optional<std::uint32_t> islandAt(const Islands& islands, Layer layer, Point at) const;
    void rebuildBoard();  // board_ = pristine_ + cracks_ + cuts_ + wires_ + bridges_
    std::optional<PinRef> padOf(const PinRef& pin) const;  // pin as a number; nullopt unknown
    // padOf of a chip's signal pin (not Power, not NoConnect); nullopt otherwise.
    std::optional<PinRef> chipSignalPin(const PinRef& pin) const;
    // The dry pin whose pad covers `at`, read as its leg: only with F.Cu active.
    std::optional<PinRef> dryLegAt(Layer layer, Point at) const;
    friend std::expected<Machine, std::vector<Diagnostic>>
    LoadMachine(const Puzzle&, const std::filesystem::path&,
                const std::optional<std::filesystem::path>&);

    Netlist design_;
    std::string name_;
    std::unique_ptr<LoadedCircuit> designCircuit_;  // design Nets: pin lookup, check-board
    Board pristine_, board_;                        // see rebuildBoard()
    std::vector<StuckPin> stuck_;
    std::vector<Crack> cracks_;
    std::vector<Crack> cuts_;
    std::vector<WireEnds> wires_;
    std::vector<WireEnds> bridges_;  // Solder Bridges: same shape as a Bodge Wire
    std::vector<PinRef> dry_;        // dry Joints, pin numbers
    std::vector<StuckCell> cells_;   // stuck cells and columns inside memory chips
    std::vector<StuckSwitch> switches_;
    BoardPower power_;  // of board_, see rebuildBoard()
};

// A cell or column Fault on `component` (Part `part`, "6530"), checked: the StuckCell, or the
// error text that follows "cell U2.D3 ram $3F: " or "column U2.D0 rom: ".
std::expected<StuckCell, std::string> ResolveCell(const CellDecl& decl, const Component& component,
                                                  std::string_view part);

// The Machine's directory (machines/NNN-name: netlist, board/, manuals/, roms/, puzzles/) of a
// file in it: a Puzzle's directory's parent, a netlist's own directory.
std::filesystem::path MachineDir(const std::filesystem::path& file);

// Read + ParsePuzzle.
std::expected<Puzzle, std::vector<Diagnostic>> ReadPuzzleFile(const std::filesystem::path& path);

// ReadPuzzleFile + LoadMachine with dir = MachineDir(path).
// `roms`: a ROM directory (ApplyRomDirectory, P5q2), applied to the design before it is built.
std::expected<Machine, std::vector<Diagnostic>>
LoadPuzzleFile(const std::filesystem::path& path,
               const std::optional<std::filesystem::path>& roms = std::nullopt);

// dir/<stem>.bwn whose dir/board/<stem>.kicad_pcb exists.
bool HasBoard(const std::filesystem::path& netlist);
// The fault-free Machine of such a netlist.
std::expected<Machine, std::vector<Diagnostic>>
LoadNetlistMachine(const std::filesystem::path& netlist,
                   const std::optional<std::filesystem::path>& roms = std::nullopt);

}  // namespace bw
