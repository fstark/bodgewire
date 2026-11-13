# Bodgewire — Naming

Style rules: [STYLE.md](STYLE.md).

## Identifiers

| Kind | Convention | Example |
|---|---|---|
| Types (class, struct, enum, alias) | `PascalCase` | `EventQueue`, `LogicValue` |
| Enum values | `PascalCase` | `LogicValue::Zero`, `DriveStrength::Weak` |
| Class methods | `camelCase` | `runUntil()`, `isFloating()` |
| Free functions | `PascalCase` | `Resolve()`, `WriteVcd()` |
| Local variables, parameters | `camelCase` | `netId`, `delay` |
| Member variables | `camelCase_` | `queue_`, `now_` |
| Public struct fields (plain data) | `camelCase` | `event.time` |
| Constants (`constexpr`, `const` at namespace scope) | `kPascalCase` | `kMaxPins` |
| Globals | `g_camelCase` | (avoid) |
| File-scope statics | `s_camelCase` | `s_partTable` |
| Namespaces | `lowercase` | `bw`, `bw::literals` |
| Files | `snake_case` | `event_queue.hpp` |
| Macros | `BW_UPPER_CASE` | (avoid) |

## Vocabulary

Code uses [GLOSSARY.md](GLOSSARY.md) terms, including its _Avoid_ lists:

- `Net`, not signal/wire/line. `Pin`, `Driver`, `Component` (placed), `Part` (catalogue).
- `LogicValue`, not level/state. `DriveStrength`, `Floating`, `Contention`.
- `Fault`, `Repair`, `Symptom`, `Instrument`, `Trigger` as defined.

## Abbreviations

Allowed: `id`, `ttl`, `mos`, `vcd`, `rng`, `ff` (flip-flop), `clk`, `led`, `asm` (assembler, assembly: the `bw asm` command). Everything else spelled out. Acronyms are words: `VcdWriter`, `TtlFamily`, not `VCDWriter`.

## Pin names

Datasheet names in upper case, active-low with `_N` suffix: `CLK`, `D`, `Q`, `Q_N`, `CLR_N`.
