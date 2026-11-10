# Recipe — Add a chip Part

A new IC in the catalogue (e.g. 74LS90, 74LS47). For an on-board LED, digit or button, see [ADD_BOARD_DEVICE.md](ADD_BOARD_DEVICE.md) instead.

Reference commit: `7fb9693` (74LS164: sequential chip). Combinational gate package: `74LS30` in `src/parts/gate_package.cpp` (a new one: `09fbb1a`, 74LS27). Falling-edge JK flip-flop with `J`/`K` sampled before the edge (`readBefore`) and X cases: `4b7cf99` (74LS107). Register of flip-flops on one shared clock and clear: `2bb1a10` (74LS175). Multiplexer whose select at X folds by trying both values, delay chosen by the input that changed: `422d284` (74LS157). Variant of an existing chip (same pinout and glyphs, other output stage), sharing its code: `e3b596b` (74LS48 beside the 74LS47).

## Which kind

- **Gate package** (only independent gates, no state): add a pin-name array, a `GateUnit` array and a `GatePackageSpec` row in `src/parts/gate_package.cpp`, then `&CreateGatePackage<"74LSxx">` in the catalogue. No new class.
- **Chip with state or special logic**: a new `Component` subclass `LsNN` in `src/parts/lsNN.{hpp,cpp}`, registered with `&CreateChip<LsNN, "74LSNN">`.

## Files

| File | Change |
|---|---|
| `src/parts/lsNN.hpp` | `class LsNN final : public Component`; comment with the datasheet pinout; `static constexpr PinIndex kX = …` (index = package pin − 1); `powerOn`, `evaluate`; state as private members |
| `src/parts/lsNN.cpp` | `LsNNPins()` builds `std::vector<PinSpec>` in pin order (`in`/`out`/`power` lambdas; clock pins: `PinSpec{"CLK", Input, Ttl, true}` = rising-edge-only); `powerOn` seeds state from `ctx.randomBit()` and `setOutput`s it; `evaluate` reads with `ctx.read`, `ctx.rose(kClk)`, `ctx.readBefore` (value before the edge, for data sampled on a clock), drives with `ctx.drive(pin, value, DelayFor(delays, value))` |
| `src/parts/delays.hpp` | `inline constexpr Delays kLsNNSomething{rise_ps, fall_ps};` nominal datasheet typicals |
| `src/parts/CMakeLists.txt` | add `lsNN.cpp` (keep sorted) |
| `src/parts/catalogue.cpp` | include + `PartInfo{"74LSNN", "lowercase description", "DIP-14"/"DIP-16", Chip, …}` in `s_parts`, **sorted by name** |
| `src/board/kicad_netlist.cpp` | only for a new package: add it to `kFootprints` (and resize the array) |
| `datasheets/74LSNN.md` | `# 74LSNN — description`, `## Function` (plain words), `## Pins` (one bullet per pin or range `` `QA`–`QH` ``, every pin name covered, `VCC`/`GND` included). Checked by `manuals: every catalogue Part has a Datasheet` |
| `tests/parts/lsNN_test.cpp` | Catch2, tag `[parts]`; `Bench` = one chip + one `Stimulus` per driven input in a `Circuit`; check truth table / sequence, clear/load priority, delays against `delays::`, X handling. Copy `tests/parts/ls164_test.cpp` |
| `tests/CMakeLists.txt` | add `parts/lsNN_test.cpp` (sorted) |
| `docs/NAMING.md` | only if a new abbreviation is used |

## Rules and gotchas

- Pin names: datasheet names, active-low suffix `_N` (`CLR_N`, `LOAD_N`); gates `1A`, `1Y`; unused pins `NCn` with `PinKind::NoConnect`.
- Open-collector outputs: `PinKind::OpenCollector` (needs a pull-up on the Board); drive 1 to release (`DriveOf` maps 1 to no drive).
- Falling-edge clocks (74LS90, 74LS93, 74LS107): `ctx.fell(pin)`; leave the pin's `risingEdgeOnly` false, so it is evaluated on both edges.
- Power Pins not at the DIP corners (74LS90, 74LS93: `VCC` pin 5, `GND` pin 10): add the Part to `kOffCornerPower` in `tests/parts/catalogue_test.cpp`.
- X on a control input makes the affected state X; never invent a 0/1.
- No zero delays, no delta cycles: every `drive` has a datasheet delay.
- Hot path: `evaluate` must not allocate (use `std::array`, no `std::vector`/`std::string`).
- Several units in one Part (74LS107): test them with `GENERATE`, not `SECTION`s inside a loop (Catch2 sections in loops are unreliable).
- `-Werror` includes unused private fields. Run `clang-format` on C++ files only, never on `CMakeLists.txt`.
- TI datasheet PDFs can't be fetched: use known typical values and say so in the commit.
