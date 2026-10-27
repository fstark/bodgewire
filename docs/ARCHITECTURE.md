# Bodgewire — Architecture

A digital circuit simulator that happens to run retro computers, not a retro computer emulator. Closer to an EDA tool with game mechanics on top than to a typical game.

Game design: [GAME_DESIGN.md](GAME_DESIGN.md).

## Stack

- C++23, CMake, a unit-test framework.
- SDL3 (window, input, audio), Dear ImGui (UI), ImPlot + custom renderer (waveforms).
- Board and schematic: custom zoomable 2D viewport on ImGui draw lists. Board layers share one frame, each with its own colour/opacity; picking on the active layer.
- Machines and scenarios described in data files (JSON or TOML).
- Pure 2D. No game engine, ECS, physics or scene graph. Fallback if ImGui limits the game UI: Qt.

## Layers

| Layer | Contents |
|---|---|
| Game / UI | panels, instruments, repair interactions, puzzles |
| Machine model | board, sockets, traces, components, faults |
| Digital simulation | event scheduler, nets, pin drivers, logic resolution |
| Component models | TTL, RAM, ROM, CPU, PLA, counters, shift registers |

Everything below Game/UI is a headless, reusable library (no ImGui/SDL/graphics). Uses: unit tests, automated fault vetting, a CLI (load machine, run diagnostic program, inject fault, dump waveforms).

```cpp
Machine m("bw80");
m.powerOn();
m.runFor(100_ms);
assert(m.video().hasValidSync());
m.injectFault(StuckAtLow{"U12", "Q3"});
m.runFor(100_ms);
// assert the specific expected symptom
```

## Simulation levels

| Level | Covers | Plan |
|---|---|---|
| L1 Logic | 4-state values; gates, flip-flops, counters, RAM, ROM, buses, tri-state; nominal propagation delays, glitches | build first |
| L2 Timing | setup/hold checks, delay spread, slow parts, timing faults | later; parameter and checking changes, not a new kernel |
| L3 Analog | rise times, capacitance, ringing, marginal levels, temperature, PSU noise | out of scope for now |

## Logic values

`0`, `1`, `Z` (high impedance), `X` (unknown/conflict): enough for floating buses, contention and open traces without analog simulation. Real floating inputs and contention are not ideal Z/X, so per-family rules apply:

| Situation | Rule |
|---|---|
| Disconnected pin (open contact), TTL input | reads 1 (internal pull-up) |
| Disconnected pin, MOS input (CPU, RAM, ROM) | last value it read |
| Undriven net with a TTL input on it | reads 1 (TTL inputs pull the net up) |
| Undriven net, MOS inputs only | last driven value (bus charge, 6502 "open bus") |
| Contention, TTL vs TTL | 0 wins (TTL sinks far more than it sources); piggybacking fixes a stuck-high output, not a stuck-low one |
| Other contention | X |
| X at a MOS input (CPU, RAM, ROM) | read as a seeded random bit; behavioural models never see X |

- Floating flag: resolution already knows when a net has no driver above Z; it marks the net *floating*. Logic still reads it per the rules above; only instruments use the flag (logic probe: no valid level, multimeter ≈ 1.4 V on a TTL net), so an open TTL input stays diagnosable.
- Power-on state: flip-flops, counters, registers and RAM start with seeded random 0/1, like real chips, never X (a counter with no reset would stay X forever). X only comes from contention.

## Pins, nets, drivers

```cpp
struct Pin    { NetId net; DriverId driver; };
struct Driver { LogicState value; DriveStrength strength; };  // 0/1/Z/X
struct Net    { std::vector<DriverId> drivers; LogicState resolved; };
```

- A net's value is the resolution of its drivers.
- Drive strength: three levels (supply > strong > weak) plus Z, from P1. Supply = short to GND/VCC; strong = chip output; weak = pull-up resistor. Open-collector output = strong 0 or Z. Higher strength wins; two strong drivers disagreeing → family contention rule (see Logic values); two supply drivers disagreeing → X.
- Resistors are weak links: a net with no strong driver takes the value across the resistor, as weak. Nets joined by resistors are resolved together as one group in the update phase, with no delay (resistors are not components). Covers pull-ups/pull-downs and resistor-isolated bus segments (ZX80: open-collector gates force NOPs onto the CPU side of the data bus while RAM drives the other side).
- Repairs and faults are distinct physical operations, even when logically similar:

| Operation | Effect |
|---|---|
| Broken / cut trace | splits a net |
| Bodge wire, short between lines | merges nets |
| Short to GND/VCC | adds a constant supply-strength driver to a net |
| Damaged output | alters one pin's driver |
| Remove / insert IC | removes / adds its drivers, inputs and faults |

- Topology is fixed during a run: board faults are applied at load, repairs are made with the power off, nets are recomputed before the next power-on.

## Faults

- The fault engine is independent of any specific machine.
- A fault is attached to a physical object (trace, joint/socket contact, pin driver, component internals), never to a symptom or a net value.
- Pin-driver faults (stuck-at, shorted pin) are generic. CPU with A4 stuck low = fault on the CPU's A4 driver: pull the CPU and A4 is no longer driven; another driver on A4 → contention.
- Internal faults live in the component model, via fault hooks (6502 with dead IRQ logic, stuck RAM cell, bad ROM bit).
- A fault = target object + effect (open, stuck at 0/1, short, extra delay, bad bit…) + activation (always, random at a rate, after T of powered-on time and until cooled down, while the board is tapped or flexed).
- Activation is evaluated in simulated time; random activations use a seeded RNG, never wall-clock time or frame rate (keeps replay deterministic).
- Conditional faults never change nets. They are limited to pin contacts and chip internals. A faulty contact (joint, socket) is a switch on its pin: when open, the pin's driver is disconnected (Z) and its input reads floating. Trace faults are permanent.

## Connectivity and visibility

| Level | Contents | Changed by |
|---|---|---|
| Design netlist | intended connections; source of the schematic | nobody (fixed per machine) |
| Physical board | copper graph (pads, track segments, vias, bodge wires, pin-to-pad contacts: solder joints, socket contacts) + fitted parts | faults, then repairs |
| Electrical nets | connected components of the copper graph; what the simulator uses | recomputed after each physical change |

- Faults and repairs edit the physical board, never the nets directly. A pin reaches its net only through a contact edge, so joint and socket faults are ordinary graph edits; `Pin::net` is derived.
- The UI must not leak faults. Board drawing, schematic, net names and net highlighting come from the design netlist and the visible physical state, so highlighting ignores breaks and the player's bodge wires. Only instruments (probe, scope, analyzer, multimeter) read electrical nets. Developer mode may show everything.
- Each fault declares its appearance: invisible, subtle (seen when zoomed in), or obvious.

## Repair validation

Golden comparison of outputs, never a per-fault check:

- Run the fault-free machine and the player's machine side by side, same seed, same inputs, on a stimulus: the machine's standard tests plus the job's complaint (e.g. the graphics demo).
- Compare only machine outputs (video, LED digits, sound), sampled at the centre of each pixel / per frame, not exact edge times, so a slower but in-spec part doesn't fail on a tiny edge shift. Internal pins are not compared: a repair may legitimately change the implementation (bypassed dead gate, rerouted signal).
- Standard tests act as diagnostics: they exercise RAM, ROM, I/O and report on the output, so internal faults become visible.
- Repaired = no divergence for N frames. Intermittent faults: several seeds or a longer run.
- Any valid fix passes (repaired trace, bodge wire, replaced chip, rerouting through a spare gate).

## Component models

- Interface: pins, internal state, `evaluate()` called when inputs change, which may only *schedule* output changes.
- Interchangeable implementations of one part, swappable without touching the circuit: behavioural (fast, default), gate-level (detailed internal faults), timing-checking (setup/hold checks). All have nominal output delays.
- CPU: behavioural, cycle-accurate at the pins, never gate/transistor level. On a clock edge it runs the cycle internally, then schedules pin changes at `now + outputDelay`. Granularity is per edge, not per cycle: inputs (data, WAIT, RDY, interrupts) are sampled on the edges the datasheet specifies, outputs driven on theirs. 6502 (BW-1, BW-2001), Z80 (BW-80). The BW-80's picture, sync included, comes from Z80 bus cycles: M1, refresh, HALT, interrupt acknowledge and I/O must be exact at the pins.
- RAM/ROM behavioural. PLAs and custom chips (ULAs) as truth tables, logic equations or behavioural models.
- Counters (e.g. 74LS161): state update on clock edge; schedule only outputs that actually change.
- Output devices are functional: the TV draws the raster from digital video + sync timing and loses lock like a real set (rolling, tearing); LED displays integrate on-time per frame (multiplexed digits).

## Simulation kernel

Discrete-event with nominal delays. Every change is an event with a real timestamp; all go through one queue. No global clock tick: falling edges, combinational changes between edges, propagation delays and asynchronous clocks must all be expressible. Components may be clock-oriented internally but only exchange pin events.

- Time: `int64` picoseconds (range ≈ 106 days).
- Delays: every output has a datasheet-typical propagation delay (≥ 1 ps), per part type, overridable per instance (slow or wrong part, timing fault). No zero-delay events, hence no delta cycles and no delta races: signals arrive in the same order as on the real board.
- Inertial delay: a new change scheduled on an output replaces that output's pending change, so a pulse shorter than the delay is swallowed, as in real TTL. The queue needs cheap cancellation (e.g. per-output generation counter, stale events skipped when popped). An evaluation compares its new output value with the *pending* value, not the current one: if they differ, the pending change is cancelled, and a new one is scheduled only if the value differs from the current output (e.g. output 0, pending 1, input goes back → cancel, schedule nothing).
- Ordering: (timestamp, phase). Never depends on insertion order.
- Each timestamp: **update** applies all net changes due, then **evaluate** runs every component whose inputs changed. Evaluations schedule changes at `now + delay`, always in the future. Order within a phase is irrelevant: updates don't evaluate, evaluations only read nets and schedule.
- Hard rule: a component never writes a net or another component's input directly; it schedules through the kernel. → deterministic, no recursive propagation.

| Case | Policy |
|---|---|
| Simultaneous input changes (A 0→1, B 1→0) | applied in the same update phase → gate evaluates once, no false glitch |
| Feedback loop (inverter fed back on itself) | oscillates at the loop delay, like the real circuit; pulse shapers and ring oscillators work |
| Clock edge and D change at the same timestamp | flip-flop samples D as it was before this timestamp (each net keeps its previous value for the current timestamp). Rare: data launched by the same clock arrives at least one delay later. Timing-checking mode: setup/hold window check, violation → X |
| Pulse shorter than a gate delay | swallowed (inertial delay) |

## Run control

- Simulated time ≠ wall-clock time: run faster than real time, pause, single-step, run until a trigger.
- Deterministic replay: same machine, faults, seed and timestamped inputs → identical event sequence. For tests and reproducing crashes.

## Instruments

- Build the logic analyzer before the game UI: it is the main tool for debugging the simulator itself.
- Instruments record only probed nets, from the moment they are probed. Normal workflow, as on a real bench: move the probes and restart the machine. Deterministic faults reproduce identically; random activations get a fresh draw on each power-on.
- In-game instruments are deliberately limited (scope: 2 channels, no memory; analyzer: few channels, no decoding). The dev analyzer is not.
- Multimeter, derived from logic state: voltage 0 ≈ 0.2 V, 1 ≈ 3.5 V, floating TTL ≈ 1.4 V, supply short = 0 V / 5 V; continuity from electrical nets (beeps through bodge wires, silent across breaks).
- The in-game analyzer samples at its own rate, derived from recorded transitions: pulses shorter than a sample period can be missed, as on a real one. Memory is a ring buffer of m samples; the trigger freezes it after the post-trigger share.

## Performance

- Scale: an 8 MHz dot clock alone is 16 M edges per simulated second (~270 k per 60 Hz frame). The whole machine is several times that, with instruments and UI live.
- Nominal delays add real glitches (a few more events); inertial delay filters the shortest ones.
- Levers: behavioural CPU/RAM/ROM (no internal events), only real output changes scheduled, only probed nets recorded, UI refreshed per frame, compact event storage (no per-event allocation).
- Goal: as fast as possible, ideally real time for a full machine with UI live.
- P1a benchmark: synthetic counter circuit, measured in applied events per second (see [KERNEL.md](p1a-kernel/KERNEL.md), S7).
- P1c benchmark: the test-pattern generator's netlist (crystal + TTL counters, no board, no UI), run headless on one thread. Pass: ≥ 10× real time. Full machines are several times larger, plus CPU, instruments and UI, so anything near 1× here means trouble later. 10× at a 14 MHz dot clock means hundreds of millions of events per second: the generator uses a low dot clock (≈ 1–2 MHz is enough for bars and a coarse grid).
- Outcome (P1c + P1d): ~5× real time; 10× not met. Accepted for P2–P3. Before the Machines milestone, measure a BW-80-like load (stub CPU driving the bus) and decide on further kernel work ([OPTIMISATION_IDEAS.md](p1d-kernel-perf/OPTIMISATION_IDEAS.md)).
- Plan for it, don't optimise early.

## Reference machines

Custom machines mimic real ones; paper-check the model against the real schematic before building each:

- PET 2001 (→ BW-2001): buses, chip selects, clock phases, TTL video counters, memory arbitration.
- ZX80 (→ BW-80): pin-exact Z80 cycles; NOPs forced through a resistor-isolated data bus; ROM address multiplexed by the refresh signal; sync from interrupt-acknowledge and I/O cycles. Reference: [Grant Searle's ZX80 page](http://searle.x10host.com/zx80/zx80.html) (schematic, circuit operation, PCB foils of both sides).

## Roadmap

| | Milestone | Content |
|---|---|---|
| P1a | Kernel | nominal inertial delays, nets, drive strengths, primitive gates/flip-flops, recording + VCD export (dev logic analyzer), event-rate benchmark. [KERNEL.md](p1a-kernel/KERNEL.md) |
| P1b | Parts + netlist | basic TTL Parts (gates, flip-flops, counters) with datasheet delays, netlist format and parser |
| P1c | Test-pattern generator netlist | the circuit as a netlist, sync checks, benchmark on it (see Performance) |
| P1d | Kernel performance | done at ~5× on the P1c benchmark (10× not met; enough for P2–P3). Further work deferred until a BW-80-like load is measured. [KERNEL_PERF_PLAN.md](p1d-kernel-perf/KERNEL_PERF_PLAN.md), [OPTIMISATION_IDEAS.md](p1d-kernel-perf/OPTIMISATION_IDEAS.md) |
| P2a | Board layout via KiCad (spike) | done: KiCad kept as layout tool; `bw export-kicad`, `.kicad_pcb` reader, copper check from geometry (`bw check-board`). [BOARD_LAYOUT.md](p2a-board-layout/BOARD_LAYOUT.md) |
| P2b | Connectors | done: `POWER` and `VIDEO` connector Parts on the Boards; outputs read through the connector; headless KiCad DRC. [CONNECTORS.md](p2b-connectors/CONNECTORS.md) |
| P2 | Test-pattern generator — **first playable** | crystal, TTL counters → H/V sync + bars/grid, TV model, see-through board view, logic probe, chip swap, trace repair. Faults: dead counter output, broken trace, stuck gate output |
| P3 | Game | scope, logic analyzer, multimeter, bodge wires, hand-placed faults (scenario files), repair validation; still on the P2 board |
| P4 | Procedural faults | random fault generation, headless vetting (reject symptom-less or trivial candidates) |
| Later | Machines | BW-1, BW-80, BW-2001: CPU models (6502, Z80), RAM/ROM, keyboards, LED display |
| Later | Timing | setup/hold checks, slow and wrong parts, timing faults |

P1–P3 test the idea, with hand-placed faults only. P1 (a–c): is the kernel fast enough? P2: can we produce boards (and schematics)? Do symptoms emerge without scripting, and can a player diagnose them? P3: is the full loop fun?

## Board authoring

KiCad is not the source of truth: its scope is real-world manufacturing, ours is "works in Bodgewire". At most it is a layout editor. Own pipeline, two separate steps:

- **Netlist** (authored, text): parts from our own chip library (only parts we model) + named nets listing pins. Own compact line format (`part U1 74LS161`, `net CLK X1.OUT U1.2 U2.2`), own parser with electronics-aware errors (unknown part, bad pin, pin on two nets). Validated headlessly with our tools (does it produce sync, pass its tests?) before any layout exists. This is the design netlist.
- **Layout**: KiCad PCB editor as layout tool only (decided in P2a, 2026-10-27). Export our netlist in KiCad netlist format (`bw export-kicad`), import it into pcbnew, place and route there (ratsnest, DRC, interactive router or Freerouting), read `.kicad_pcb` back (footprint positions, track segments, vias). No schematic editor, no circuit editing. Projects in `boards/<name>/`; steps in [KICAD_GUIDE.md](p2a-board-layout/KICAD_GUIDE.md).
  - P2a result: counter and test-pattern Boards laid out by a KiCad newcomer (KiCad 10, test pattern auto-routed by Freerouting: 479 segments, 13 vias), both pass `bw check-board`. Pain points were one-offs or guide material: library table path after install, Java for Freerouting, ratsnest redrawn differently while moving, outline drawn on F.Cu by mistake, one connection left by Freerouting.
  - Next: everything after the hand layout is automated (export, read, check; KiCad's DRC headless via `kicad-cli pcb drc` in the tests). Boards need connectors as Parts in the netlist: power entry (VCC, GND) and outputs (video), so they get footprints, pads and Traces like any chip.
- Whatever the layout source, Bodgewire checks the copper connects exactly what the netlist says.
- Schematic generated from the netlist (net labels, no wire routing, grouped by function).
- Layout quality is a visual problem only; improvable later without touching simulation.
- Risk: dense boards the router can't finish (BW-80 on 2 layers). Fallbacks: extra layer, wire jumpers (common on period boards).
