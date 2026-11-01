#pragma once

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
#include "machine/puzzle.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"

namespace bw {

class Machine;

// A Bodge Wire with its ends as pin numbers.
struct WireEnds {
    PinRef a, b;
    BodgeWire wire;  // pad positions
};

// root/netlists/<machine>.bwn + root/boards/<machine>/<machine>.kicad_pcb; all semantic errors.
std::expected<Machine, std::vector<Diagnostic>> LoadMachine(const Puzzle& puzzle,
                                                            const std::filesystem::path& root);

// Design netlist + Board + Faults. Repairs edit it; build() makes the Circuit for one power-on.
class Machine {
public:
    std::string_view name() const {
        return name_;
    }  // never the Puzzle's file name: it names the Fault
    bool swap(std::string_view reference);  // chip swap; false if no such Component
    bool mend(Layer layer, Point at);       // removes cracks and Cuts within 1 mm
    bool cut(Layer layer, Point at);        // false: no straight Trace there in the current Copper
    bool addWire(const PinRef& a, const PinRef& b);       // false: unknown pad, or the same pad
    bool removeWire(const PinRef& a, const PinRef& b);    // either order; false: no such wire
    bool removeBridge(const PinRef& a, const PinRef& b);  // either order; false: no such bridge
    std::span<const CrackDecl> cuts() const { return cuts_; }
    std::span<const WireEnds> wires() const { return wires_; }
    std::span<const WireEnds> bridges() const { return bridges_; }
    LoadResult build() const;                      // electrical Circuit with the current Faults
    const Board& board() const { return board_; }  // with cracks, Cuts and Bodge Wires
    const Board& pristineBoard() const { return pristine_; }  // fault-free Copper, for the view
    const LoadedCircuit& designCircuit() const { return *designCircuit_; }

    // Electrical Net under `at` in `built` (this Machine's build()), in the current Copper: a pad
    // or via on either Layer, else a Trace on `layer`. nullopt off Copper or on an unnamed island.
    std::optional<NetId> probeNet(const LoadedCircuit& built, Layer layer, Point at) const;
    // Position of a Pin's pad: "U1", "QA" or "14"; nullopt if unknown.
    std::optional<Point> padPosition(std::string_view reference, std::string_view pin) const;
    // Same copper island under both points, in the current Copper (continuity).
    bool connected(Layer layerA, Point a, Layer layerB, Point b) const;

private:
    Machine() = default;
    // Island of the current Copper under `at`; nullopt off Copper.
    std::optional<std::uint32_t> islandAt(const Islands& islands, Layer layer, Point at) const;
    void rebuildBoard();  // board_ = pristine_ + cracks_ + cuts_ + wires_ + bridges_
    std::optional<PinRef> padOf(const PinRef& pin) const;  // pin as a number; nullopt unknown
    friend std::expected<Machine, std::vector<Diagnostic>>
    LoadMachine(const Puzzle&, const std::filesystem::path&);

    Netlist design_;
    std::string name_;
    std::unique_ptr<LoadedCircuit> designCircuit_;  // design Nets: pin lookup, check-board
    Board pristine_, board_;                        // see rebuildBoard()
    std::vector<StuckPin> stuck_;
    std::vector<CrackDecl> cracks_;
    std::vector<CrackDecl> cuts_;  // line 0
    std::vector<WireEnds> wires_;
    std::vector<WireEnds> bridges_;  // Solder Bridges: same shape as a Bodge Wire
};

// The directory holding netlists/, boards/, puzzles/, manuals/, datasheets/: the file's
// directory's parent.
std::filesystem::path MachineRoot(const std::filesystem::path& file);

// Read + ParsePuzzle.
std::expected<Puzzle, std::vector<Diagnostic>> ReadPuzzleFile(const std::filesystem::path& path);

// ReadPuzzleFile + LoadMachine with root = MachineRoot(path).
std::expected<Machine, std::vector<Diagnostic>> LoadPuzzleFile(const std::filesystem::path& path);

// root/netlists/<stem>.bwn whose root/boards/<stem>/<stem>.kicad_pcb exists.
bool HasBoard(const std::filesystem::path& netlist);
// The fault-free Machine of such a netlist.
std::expected<Machine, std::vector<Diagnostic>>
LoadNetlistMachine(const std::filesystem::path& netlist);

}  // namespace bw
