# P5z2 Kernel: Parts — Design

Spec: [KERNEL_PARTS.md](KERNEL_PARTS.md). Plan: [KERNEL_PARTS_PLAN.md](KERNEL_PARTS_PLAN.md). Recipe: [ADD_PART.md](../recipes/ADD_PART.md) (gate packages; Rules, Quiet Pins). Methods: P5z's trace check and measurement (`bench/trace_check.sh`, `bench/measure.sh`, [KERNEL_BW2001_DESIGN.md](../p5z-kernel-bw2001/KERNEL_BW2001_DESIGN.md)), unchanged.

## Files

| File | Change |
|---|---|
| `src/parts/gate_package.{hpp,cpp}` | A |
| `src/parts/ls93.cpp`, `src/parts/ls107.cpp`, `src/parts/ls90.cpp` (if the 93's gains) | B |
| `src/parts/ls373.{hpp,cpp}` | C |
| `src/sim/component.hpp`, `src/sim/simulator.hpp` | A: `EvalContext::readChanged` |
| `tests/parts/gate_package_test.cpp`, `tests/parts/ls373_test.cpp` | A, C |
| `docs/recipes/ADD_PART.md` | A (the gate package row), C (Quiet Pins examples) |
| `docs/ARCHITECTURE.md`, `docs/p1d-kernel-perf/OPTIMISATION_IDEAS.md` | last phase |

## A. Gate packages, unit by unit

- `GatePackage` gains `bool first_` (true from `powerOn`, which it now overrides, and false after the first evaluation).
- `GatePackageSpec::evaluate`'s signature becomes `void (*)(EvalContext&, bool all)`, so the specialised function knows about the power-on evaluation without a member of its own.
- `EvaluatePackage<Units, PartDelays>(ctx, all)`, per unit:
  - **Skip:** if `!all` and no input's Net has `changedAt == now`, skip the unit. This is the test `readBefore` makes.
  - **Otherwise:** drive as today.
  - **Cost:** each input's Net is read once, value and `changedAt` together, through a new inline `EvalContext` member `readChanged(pin, LogicValue& value) -> bool` (the value, and whether it changed at this timestamp; `now_` read inside, as `readBefore` does). `EvalContext::now()` is out of line, so it is not compared per input. This is an addition to `EvalContext`, allowed since P5z (additions only).
- **Why exact:** every change of a unit's input Net wakes the package (gate inputs are ordinary Pins, never sampled or quiet), and at that timestamp the change test is true. So every unit is evaluated at every change of its inputs, and its last drive is the value its current inputs give. A skipped unit would drive that value again, a no-op for the inertial `drive` (equal to what is current or pending).
- `Gate` (`src/sim/primitives`), the single gate used in tests and small netlists, is not touched.

## B. 74LS93 and 74LS107

- **74LS93:** `r0` is `One` when both reset reads are `One`, `Zero` when either is `Zero`, `X` otherwise. That is `GateFunction(And, …)` written for two inputs. No change elsewhere.
- **74LS107:** in `evaluate`, at a falling clock with the clear inactive:
  - if `readBefore(J)`, `readBefore(K)` and `state` are all 0 or 1, the next state is `NextKnown(j, k, q)`;
  - otherwise `Next()` as today.
- The 74LS90 shares the 93's reset form (`r0`, `r9`), so it gets the same change only if the 93's version shows a gain worth it.

## C. Quiet D on the 74LS373

- Member `bool quiet_` (false from `powerOn`) and a helper `setQuiet(ctx, on)` over `kD`, as `Ram2114` has.
- At the end of `evaluate`: `const bool closed = le == Zero;` and if `closed != quiet_`, call `setQuiet(ctx, closed)`.
- **Why exact:** with `LE` a definite 0, the loop's `le == Zero` branch reads `D` only under `fell(kLe)`, which is an `LE` wake (loud), and through `readBefore`, a Net read. Every other `D` change while closed leaves the latch and outputs as they are, and the inertial `drive` makes re-driving them a no-op. `LE` X stays loud: that branch reads `D` as a level.
- `LE` and `OE_N` stay loud.
- No timing state is kept from `D`, so `lastChange` is not needed.

## ADD_PART

- Gate package row: `evaluate` takes `all`, and a unit is evaluated only when one of its inputs changed (or `all`). A new package needs nothing more than its units row.
- Rules, Quiet Pins, reworded (line 41 allows only a deselect; a closed latch is not one): a Pin may be quiet under a definite level of a control Pin while `evaluate` reads it only through `readBefore` at a loud Pin's edge, or not at all; the controls stay loud. Examples: the 2114 and ROM deselected, the 74LS244's disabled halves (P5z), the 74LS373's closed latch.
