# Connectors — Detailed Design (P2b)

Implements the specification in [CONNECTORS.md](CONNECTORS.md) (S1–S5).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## 1. Files

```
src/parts/connector.hpp/.cpp      new: Connector Component, ConnectorPins()
src/parts/catalogue.hpp/.cpp      PartKind::Connector; POWER, VIDEO entries
src/board/kicad_netlist.cpp       footprints for packages KK-2, KK-3
src/board/board_check.hpp/.cpp    BoardNetlistProblems()
src/cli/main.cpp                  export-kicad, check-board call BoardNetlistProblems()
tests/CMakeLists.txt              headless DRC tests (S5)
netlists/*.bwn, boards/*          J1 (and J2) added; Boards re-laid by the user
tests/machines/test_pattern_test.cpp   outputs read through J2 (S4)
```

## 2. Connector Component (S1)

```cpp
// parts/connector.hpp
// Wire-to-board header; passive: Pins are loads (Input) or power, no behaviour.
class Connector final : public Component {
public:
    Connector(std::string reference, std::vector<PinSpec> pins);
    void evaluate(EvalContext&) override {}
};

std::vector<PinSpec> ConnectorPins(std::string_view part);  // "POWER", "VIDEO"; Check()s the name
```

| Part | Package | Pins |
|---|---|---|
| `POWER` | `KK-2` | VCC (Power), GND (Power) |
| `VIDEO` | `KK-3` | VIDEO (Input), SYNC_N (Input), GND (Power) |

All `Family::Ttl`. Power Pins reuse the builder's implicit power (`addImplicitPower()`), so `part J1 POWER` alone puts J1.1 on VCC and J1.2 on GND. Input Pins wake the Connector on Net changes; `evaluate` is empty (cost: one virtual call per SYNC_N/VIDEO change, negligible).

Catalogue: `enum class PartKind { Chip, Resistor, Connector }`; entries (sorted by name):

```cpp
PartInfo{"POWER", "5 V power connector", "KK-2", Connector, &CreateConnector<"POWER">},
PartInfo{"VIDEO", "TTL monitor connector", "KK-3", Connector, &CreateConnector<"VIDEO">},
```

`CreateConnector<Name>` rejects parameters like `CreateChip`.

## 3. Footprints (S1)

`kFootprints` in `kicad_netlist.cpp` gains:

```
KK-2 → Connector_Molex:Molex_KK-254_AE-6410-02A_1x02_P2.54mm_Vertical
KK-3 → Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical
```

Pads "1".."3", through-hole roundrect/oval: already supported by the reader.

## 4. Board rules (S2)

```cpp
// board_check.hpp — netlist rules for a Board; empty = ok.
std::vector<std::string> BoardNetlistProblems(const LoadedCircuit& loaded);
```

Counts `loaded.parts` with `info->name == "POWER"`; ≠ 1 → `"a board needs one POWER connector"`. `CheckBoard` stays geometry-only. CLI: `export-kicad` prints the problems and exits 1 without writing; `check-board` prints them with the copper problems.

## 5. Outputs through the connector (S4)

`test_pattern_test.cpp`: find the `LoadedPart` with `info->name == "VIDEO"`, take its `Component*` from `loaded.components`, and the Nets of its Pins `VIDEO` / `SYNC_N` via `circuit.netOf(PinId{firstPin + pinIndex("VIDEO")})`.

## 6. Headless DRC (S5)

```cmake
find_program(BW_KICAD_CLI kicad-cli PATHS /Applications/KiCad/KiCad.app/Contents/MacOS)
if(BW_KICAD_CLI)
  file(GLOB boards ${PROJECT_SOURCE_DIR}/boards/*/*.kicad_pcb)
  foreach(board ${boards})
    get_filename_component(name ${board} NAME_WE)
    add_test(NAME drc_${name} COMMAND ${BW_KICAD_CLI} pcb drc --severity-error
             --exit-code-violations -o ${CMAKE_CURRENT_BINARY_DIR}/${name}_drc.rpt ${board})
  endforeach()
endif()
```

No `kicad-cli`: no DRC tests (the rest of the suite is unaffected).

## 7. Testing

- `tests/parts/connector_test.cpp`: pin lists of POWER and VIDEO; parameters rejected; evaluate does nothing (Nets unchanged).
- `builder_test`: `part J1 POWER` alone → J1.1 on VCC, J1.2 on GND.
- `kicad_netlist_test`: footprint table entries KK-2, KK-3.
- `board_check_test`: `BoardNetlistProblems` with 0, 1, 2 POWER Parts.
- Integration: `boards_test` (updated Boards), `test_pattern_test` through J2, DRC tests.
