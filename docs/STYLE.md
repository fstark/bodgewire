# Bodgewire — Code Style

Naming rules: [NAMING.md](NAMING.md). Terminology: [GLOSSARY.md](GLOSSARY.md).

## Language Standard

- C++23. Compilers: Apple clang ≥ 17, clang ≥ 17, GCC ≥ 14.
- Modern interfaces: `std::string_view` for read-only strings, `std::span` for read-only ranges, `enum class`, `constexpr`, return values instead of output parameters.
- No `#define` constants or function-like macros. Macros only for things the language can't do (none yet).
- No raw owning pointers: `std::unique_ptr` / containers. Raw pointers and references are non-owning.
- No C-style casts: `static_cast`, `std::bit_cast`.
- Fixed-width integers (`std::int64_t`, `std::uint32_t`) for stored data; `std::size_t` for sizes and indices into containers.
- IDs are strong types: `enum class NetId : std::uint32_t {};`, never a bare integer.

## Errors

- Build-time misuse (connecting a pin twice, unknown pin name): `Check(condition, message)` (free function, `std::source_location`) that throws `std::logic_error`, active in all builds. Cost is irrelevant outside the run loop.
- Input from files (netlists, scenarios): `std::expected<T, Error>` with a message naming the file, line and object.
- Hot path (run loop, resolution, scheduling): `assert` only; no exceptions, no allocation per event.

## Formatting

- `clang-format` with the repo's `.clang-format` is authoritative. Summary: 4-space indent, 100 columns, braces on the same line, `int* p` (pointer binds to the type).
- `#pragma once` in every header.
- Include order: own header, standard library, third-party, project. Project includes are relative to `src/` (`#include "sim/circuit.hpp"`).

## Files and layout

- Headers `.hpp`, sources `.cpp`, one main type per file pair.
- Library code under `src/<module>/`, tests under `tests/<module>/`, benchmarks under `bench/`.
- The `sim` module (namespace `bw`) depends on the standard library only: no SDL, ImGui, or I/O beyond `std::ostream`.

## Comments

- Say what the code can't: why, units, invariants, datasheet references. One short line.
- Units are always in names or comments: all `Time` values are picoseconds.

## Tests

- Catch2 v3. One test file per source file: `tests/sim/circuit_test.cpp` for `src/sim/circuit.cpp`.
- Test names: `"<module>: <behaviour>"`, e.g. `"resolve: TTL contention reads 0"`. Tags: `[sim]`, `[slow]`.
- Tests are deterministic: fixed seeds, no wall-clock time.
