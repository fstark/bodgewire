# Bodgewire — Architecture

A digital circuit simulator that happens to run retro computers, not a retro computer emulator. Closer to an EDA tool with game mechanics on top than to a typical game.

Game design: [GAME_DESIGN.md](GAME_DESIGN.md).

## Stack

- C++23, CMake, a unit-test framework.
- SDL3 (window, input, audio), Dear ImGui (UI), ImPlot + custom renderer (waveforms).
- Board view: custom zoomable 2D viewport on ImGui draw lists. Board layers share one frame, each with its own colour/opacity; picking on the active layer.
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
- Drive strength: three levels (supply > strong > weak) plus Z, from P1. Supply = short to GND/VCC; strong = chip output; weak = pull-up resistor. Open-collector output = strong 0 or Z. A bidirectional pin (P4n: RAM data pins) is a tri-state Driver and a reader of its net at once, released (Z) at power-on. Higher strength wins; two strong drivers disagreeing → family contention rule (see Logic values); two supply drivers disagreeing → X.
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
- Power (P3f, [SHORTS_POWER.md](p3f-shorts-power/SHORTS_POWER.md)): each power Pin of a Part declares the voltage it expects; a Rail is a Net holding a `POWER` connector supply pin, at that pin's voltage. Rails are static per build (fixed topology), so "Powered" is worked out at build from the electrical Nets, not simulated: a chip, or a device fed through its Connector (P4i paddle controller), is Powered when every power Pin sits on a Rail of exactly its voltage, otherwise its outputs are Z and its inputs ignored (cracked VCC Trace, chip on the wrong Rail). A Net holding supply pins of different voltages is a Dead Rail (bench supply in current limit: 0 V, nothing damaged). Rules in volts, so more Rails (12 V, −5 V) are data only. Only intermittent power contacts will need runtime checks, through the existing contact-switch mechanism.

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
| Design netlist | intended connections; source of Net names, pinouts, Driver marks and the manual checks | nobody (fixed per machine) |
| Physical board | copper graph (pads, track segments, vias, bodge wires, pin-to-pad contacts: solder joints, socket contacts) + fitted parts | faults, then repairs |
| Electrical nets | connected components of the copper graph; what the simulator uses | recomputed after each physical change |

- Faults and repairs edit the physical board, never the nets directly. A pin reaches its net only through a contact edge, so joint and socket faults are ordinary graph edits; `Pin::net` is derived.
- The UI must not leak faults. Board drawing, net names, net highlighting, pinouts and Driver marks come from the design netlist and the visible physical state, so highlighting ignores breaks and the player's bodge wires. Only instruments (probe, scope, analyzer, multimeter) read electrical nets. Developer mode may show everything.
- Each fault declares its appearance: invisible, subtle (seen when zoomed in), or obvious.

## Repair validation

Dropped (2026-11-01, P3f): no win condition for now; the player decides when the Machine is fixed. A golden comparison of outputs can't be trusted on larger Machines (a fault seen only under one specific use, such as a program touching one bad RAM chip), and a per-fault check rejects valid workarounds. Revisit later.

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
- Where instruments go (decided 2026-10-29, realism): held tips (logic probe, multimeter leads) touch any copper: pads, Traces, vias. Attached leads (logic analyzer Clips, oscilloscope Hooks) go on pads only, like real grabbers; a click elsewhere is refused with the reason on the toolbar.
- Each instrument with a window carries its tool's button in that window; picking the tool opens the window. Focusing a window never changes the tool.
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
- BW-80-like load (P5a, [BUS_LOAD.md](p5a-bus-load/BUS_LOAD.md)): `netlists/bus_load.bwn`, a stub Z80 at ZX80 timing with its bus, decoding and video, runs at 0.22× (120 applied events per simulated µs, the per-event cost of P1d). The buses are two thirds of the events. Kernel work (P5b) comes before the CPU Parts; this netlist is its benchmark.
- P5b ([KERNEL_BUSES.md](p5b-kernel-buses/KERNEL_BUSES.md), Results): `bus_load` 0.40× (424 instructions, 90 cycles an applied event), the test pattern 7.35×, without changing the Part API, the traces or which Nets are simulated. Further gains bend the design; the user chose to go on to the 6502 first and measure the BW-2001 and the real Z80 (OPTIMISATION_IDEAS, Next steps).
- Output Devices and instruments stay off the simulation thread: the simulation only appends (time, value) Samples of watched Nets (~5–10 instructions per change, ~0.1% on the test pattern); consumers decode them at their own pace. Headless (P2c): one thread, the Recorder trimmed after each slice. With the UI: the Recorder stages one slice, then its Samples go into one single-producer/single-consumer lock-free ring per watched Net; the simulation waits when a ring is full rather than drop edges. All consumers run on one devices thread (not the UI thread, so UI stalls don't stall the simulation), which fans each Sample out to every consumer of its Net. Device models (e.g. `Tv`) take timed edges only, so they move threads unchanged.
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
| P2c | TV | done: monitor model on the VIDEO connector (`src/devices`): sync separation, triggered deflection (rolls when sync is wrong), frame buffer; `bw run --tv` writes a BMP; no measurable cost on the simulation. [TV_PLAN.md](p2c-tv/TV_PLAN.md) |
| P2d | Faults, repairs, logic probe | done: headless Machine model (`src/machine`): Nets from the Board's Copper; Stuck-at Faults and Broken Traces in `.bwp` Puzzle files (three in `puzzles/`); chip swap and Trace mend; TTL logic probe (`src/instruments`); `bw run x.bwp --repair --probe`; checked on the TV picture. [FAULTS_PLAN.md](p2d-faults/FAULTS_PLAN.md) |
| P2e | UI shell | done: `bw_ui` (SDL3 + ImGui, fetched by CMake) shows the live TV picture with power, pause, seed and speed controls; `src/runner`: simulation and devices threads, one SPSC ring per watched Net (counted watches), triple-buffered TV frames; identical frames to the headless path; test pattern at 1× live, `Max` ≈ 5.5×. [UI_SHELL_PLAN.md](p2e-ui-shell/UI_SHELL_PLAN.md) |
| P2f | Board view | done: see-through Board drawn from the `.kicad_pcb`: zoom, pan, hover, design-Net highlight; cracks visible when zoomed in; TV optional. Required: `bw_ui --screenshot FILE.png --after 1s` (saves its own window, then exits), so the agent can check visuals without macOS screen-recording permission. [BOARD_VIEW.md](p2f-board-view/BOARD_VIEW.md) |
| P2g | Board probe | done: logic probe by clicking the running Board (electrical Nets, one watched Net). [BOARD_PROBE.md](p2g-board-probe/BOARD_PROBE.md) |
| P2h | Board repair | done: Swap and Mend tools with the power off, repair log, Nets rebuilt at power-on (`src/session`), `bw_ui --repair`: completes the first playable. [BOARD_REPAIR_PLAN.md](p2h-board-repair/BOARD_REPAIR_PLAN.md) |
| P2 | Test-pattern generator — **first playable** | done 2026-10-28: the three Puzzles played end to end in `bw_ui` (works, but the UI is hard to use). P2a–P2h together: crystal, TTL counters → H/V sync + bars/grid, TV model, see-through board view, logic probe, chip swap, trace repair. Faults: dead counter output, broken trace, stuck gate output |
| P3a | UI usability | done 2026-10-28: Puzzle Brief, User and Service Manuals and Datasheets (authored, checked against the netlist), state banner, toolbar, Chip panel, pin names on the Board, snapping, grouped layout, navigation (`src/manual`). [UI_USABILITY_PLAN.md](p3a-ui-usability/UI_USABILITY_PLAN.md) |
| P3b | Net panel | done 2026-10-29: click a pin or Trace to select its Net; it stays highlighted and the `Net` section lists its Drivers and Loads (`U6` pin 6 1Y → `U2` pin 9, `U9` pin 5), each a link that frames it on the Board; design Netlist only (`src/netlist/net_pins`). Replaces the schematic view (see Board authoring). Later: consistent Chip/Net selection, floating info windows, less hover. [NET_PANEL_PLAN.md](p3b-net-panel/NET_PANEL_PLAN.md) |
| P3c | Logic analyzer | done 2026-10-29: 8 Channels × 4096 points clipped on pads (`Clip (C)`), per-Channel Trigger, floating `Logic Analyzer` window, `bw_ui --clip`; electrical Nets, 1.00× with 8 Clips at 100 MHz. Later: discoverability, refusal feedback, deeper memory, navigation bugs (done in P5e). [LOGIC_ANALYZER_PLAN.md](p3c-logic-analyzer/LOGIC_ANALYZER_PLAN.md) |
| P3d | Oscilloscope + multimeter | done 2026-10-31: 2 Hooks on pads, exact edge timing, live sweeps; held meter leads anywhere: logic-state voltages, continuity with the power off; the placement rule applied to the analyzer too; 1.00× with every instrument attached. [SCOPE_METER_PLAN.md](p3d-scope-meter/SCOPE_METER_PLAN.md) |
| P3e | Bodge wires + trace cuts | done 2026-10-31: new Repairs through the Session: `Cut (K)` on straight Traces (a player-made crack, mended like one; also snips wires), `Wire (W)` between two pads; drawn, but Net highlighting ignores them; continuity follows them at once. [BODGE_WIRES_PLAN.md](p3e-bodge-wires/BODGE_WIRES_PLAN.md) |
| P3f | Solder bridges + real power | done 2026-11-01: Solder Bridges between neighbouring pins on the solder side (subtle, removed with Cut); chips Powered only through their power Pins, worked out per build in volts; Dead Rail with a current-limit LED; four new Puzzles on the test-pattern board. Repair Validation dropped; Wrong Part, sockets, chip removal later. [SHORTS_POWER_PLAN.md](p3f-shorts-power/SHORTS_POWER_PLAN.md) |
| P3 | Game | scope, logic analyzer, multimeter, bodge wires, hand-placed faults (scenario files); still on the P2 board. P3a–P3f together answer: is the full loop fun? |
| P4 | Tutorial ladder code | one step per ladder Machine that needs new code (see [GAME_DESIGN.md](GAME_DESIGN.md) Machines); each step ends with that Machine running and its Puzzles playable. Machines needing no new code have only a spec in `docs/machines/`. Each Machine has its spec and design in `docs/machines/NNN-name/`; each step's own directory holds only the code change and the Machine's tests |
| P4a | Counter LED | done 2026-11-01: on-board LED Part (lit while current flows, drawn lit on the Board, half-lit when switching fast); counter reworked to 20 bits + LED, new Board, manuals, three Puzzles. [COUNTER.md](machines/010-counter/COUNTER.md), [COUNTER_LED.md](p4a-counter-led/COUNTER_LED.md), [COUNTER_LED_PLAN.md](p4a-counter-led/COUNTER_LED_PLAN.md) |
| P4b | LED chaser (020) | done 2026-11-01: 74LS164 shift register Part; `kMaxWatched` 16 → 32 (8 LEDs = 16 Nets, plus instruments; 1.00× with every instrument attached); chaser netlist, Board, manuals, four Puzzles. [LED_CHASER.md](machines/020-led-chaser/LED_CHASER.md), [SHIFT_REGISTER.md](p4b-shift-register/SHIFT_REGISTER.md), [SHIFT_REGISTER_PLAN.md](p4b-shift-register/SHIFT_REGISTER_PLAN.md) |
| P4c | Manual cleanup | done 2026-11-01: User Manual cut to a short outside description (User Manual window only); Service Manual cut to Component roles (Chip panel only); Board hover shows the datasheet description; Net panel and Net selection removed; hovering a Net marks its Driver pads; easy/hard modes dropped. [MANUAL_CLEANUP.md](p4c-manual-cleanup/MANUAL_CLEANUP.md) |
| P4d | Dice (030) | on-board push-button Part (pressed by clicking it on the Board while powered; bounce-free first), button state through the Session into the simulation; mod-6 counter + LED decoding from existing Parts; netlist, Board, manuals, Puzzles |
| P4e | Crack placement | done 2026-11-02: a Broken Trace is placed by two pads and a position along the crackable Copper between them (`fault crack U1.QA U13.2B 79%`), not by a Board point, so Puzzles survive a new layout (only which other pads lose the signal may change); placement skips copper near pads, vias, corners and junctions; existing Puzzles and tests converted. [CRACK_PLACEMENT.md](p4e-crack-placement/CRACK_PLACEMENT.md), [CRACK_PLACEMENT_PLAN.md](p4e-crack-placement/CRACK_PLACEMENT_PLAN.md) |
| P4f | Stopwatch (040) | done 2026-11-08: 74LS90 decade counter (falling-edge clocks: `EvalContext::fell`) and 74LS47 decoder (open-collector outputs) Parts; a common-anode 7-segment DIGIT Part drawn lit on the Board, upright on its pad 1 row; LEDs generalised to lamps listed in the catalogue; watch budget 48. The stopwatch: 0.00–9.99 s from a 1 MHz clock, Start, Stop and Reset buttons, netlist, Board, manuals, six Puzzles. [SEVEN_SEGMENT.md](p4f-seven-segment/SEVEN_SEGMENT.md), [SEVEN_SEGMENT_PLAN.md](p4f-seven-segment/SEVEN_SEGMENT_PLAN.md), [STOPWATCH.md](machines/040-stopwatch/STOPWATCH.md) |
| — | Test-pattern generator (050) | done 2026-11-08: no new code; the P2 Machine and its eight Puzzles specified under `docs/machines/050-test-pattern/`, with a new Puzzle, Zips (the line counter's enables cut off `LINE_END`). [TEST_PATTERN.md](machines/050-test-pattern/TEST_PATTERN.md), [TEST_PATTERN_DESIGN.md](machines/050-test-pattern/TEST_PATTERN_DESIGN.md) |
| P4g | Bouncing ball (060) | done 2026-11-09 (user check pending): 74LS27 triple 3-input NOR, 74LS93 4-bit ripple counter and 74LS107 dual JK flip-flop Parts. The bouncing ball: a 2 MHz dot clock, a box of four walls and a dashed net, a 2-dot × 8-line ball moving by Pong-style counter reloads and bouncing off each wall through JK direction flip-flops; a 74LS30 as the video mixer; TV unchanged; 26 chips, netlist, Board, manuals, five Puzzles; 1.00× with every instrument. [BALL_PARTS.md](p4g-ball-parts/BALL_PARTS.md), [BALL_PARTS_PLAN.md](p4g-ball-parts/BALL_PARTS_PLAN.md), [BOUNCING_BALL.md](machines/060-bouncing-ball/BOUNCING_BALL.md) |
| P4h | Pin faults | done 2026-11-09 (user check pending): two Faults of one IC pin, nothing drawn on the Board. A stuck input (`fault stuck REF.PIN 0\|1` on a chip input: the chip reads the value, its Net unchanged; swap). A dry Joint (`fault joint REF.PIN`: the pin off its pad's Net, it floats; repaired by `Machine::resolder`, a Session Repair, `--repair resolder:REF.PIN`, the Resolder (R) tool; a swap does not fix it). Leg or pad: a touch on a chip pad reads the leg with F.Cu active, the pad with B.Cu; continuity from a dry leg is open; the probe panel and meter labels say "leg" or "pad"; the Probe hint says it (no bench manual yet). Puzzles: 060 Solid net (dry Joint at `U15` pin 13, replacing Wide net), 040 Missing eight (`U8` input `D` stuck at 0). Intermittent Joints stay Later. [PIN_FAULTS.md](p4h-pin-faults/PIN_FAULTS.md), [PIN_FAULTS_PLAN.md](p4h-pin-faults/PIN_FAULTS_PLAN.md) |
| P4i | Knob meter (045) | done 2026-11-09 (user check pending): `PADDLE` controller behind a 4-pin Connector (`TRIG_N`, `OUT`, `VCC`, `GND`), the logic-only pot + 555 one-shot: each fall of `TRIG_N` starts a pulse from `min` to `max` as the Knob is turned; Powered from the Board like a chip. The Knob through the Session (side-panel slider, mouse wheel, Up/Down; `--knob J2:40%` in `bw run` and `bw_ui`), kept across power and Repairs. 74LS175 latch Part. The Knob meter: a 100 ms cycle triggers the controller, the pulse gates a 10 kHz clock into two 74LS90s, two 74LS175s latch the count, two digits show 00–99; netlist, Board, manuals, five Puzzles (Dead knob, Runaway, Single figures, Odd tens, Four to seven). New recipe [ADD_CONNECTOR_DEVICE.md](recipes/ADD_CONNECTOR_DEVICE.md). [KNOB_METER.md](machines/045-knob-meter/KNOB_METER.md), [PADDLE.md](p4i-paddle/PADDLE.md), [PADDLE_PLAN.md](p4i-paddle/PADDLE_PLAN.md) |
| P4j | Paddle tennis (070) | done 2026-11-10 (user check pending): 74LS157 multiplexer and 74LS48 decoder Parts; `PADDLE … reversed=yes`; with two knobs, Q/A turn the first and Up/Down the second, keys at 100 %/s. The 060 ball and box plus two paddles from two controllers triggered from `VSYNC`, hits leaving at the angle of the paddle quarter hit (4 angles), a speed-up after two turns, misses served from the loser's edge after about 0.6 s, scores to 11 drawn on the TV (two 74LS90s multiplexed by 74LS157s into one 74LS48, segments gated into rectangles), game over with the ball bouncing alone, a Start button, random power-on scores. 58 chips, netlist grown in six tested stages, Board, manuals, Session test; 1.46× headless, 1.00× in `bw_ui`; sound left out; Puzzles in their own row. [TENNIS_PARTS.md](p4j-tennis-parts/TENNIS_PARTS.md), [TENNIS.md](machines/070-tennis/TENNIS.md), [TENNIS_PARTS_PLAN.md](p4j-tennis-parts/TENNIS_PARTS_PLAN.md) |
| — | Paddle tennis Puzzles (070) | done 2026-11-10 (user check pending): no new code; nine Puzzles, mostly on the picture: Mirror paddle (`U31` select stuck), Shy paddles (bridge `LPADV`–`RPADV`), Paddle fence (dry Joint on a column decode: four paddles), Long paddle (paddle counter clock cracked), Invisible ball (`SERVING` cracked at the ball's gate), Double scores (`HX` stuck), Folded digits (`U54` select stuck), Rolling picture (dry Joint on `VSYNC_N` at the sync mixer), Lost paddles (dry Joint on the frame-end decode: 308-line frames, no sync, no paddles, no sideways motion). [TENNIS.md](machines/070-tennis/TENNIS.md) (Puzzles), [TENNIS_DESIGN.md](machines/070-tennis/TENNIS_DESIGN.md) (Puzzles) |
| P4k | Board placement | done (2026-11-11, user check pending): `bw place netlists/<m>.bwn boards/<m>/<m>.place -o boards/<m>/<m>.kicad_pcb` writes the placed Board (vendored footprints, pad nets, outline, no tracks); panel parts by `edge`/`row`/`next` rules, chips annealed on wire length in one orientation. 070 regenerated (11.07 m against 15.1 m by hand) and routed by the user, the nine Puzzles unchanged. Functional groups later. [BOARD_PLACEMENT.md](p4k-board-placement/BOARD_PLACEMENT.md), [design](p4k-board-placement/BOARD_PLACEMENT_DESIGN.md), [plan](p4k-board-placement/BOARD_PLACEMENT_PLAN.md) |
| P4l | Message display (080) | done (2026-11-11, checked by the user): one ROM model behind a spec table (2716 EPROM, 2513 character generator with its real pinout), contents from a `.rom` text file in `roms/` named by `contents=`, resolved against the netlist's directory; 74LS165; DIP-24 package. 080 shows a 16 × 16 page (a Dickinson stanza) at 2 MHz; netlist, manuals, Board placed by `bw place` and routed by the user, no Puzzles yet. [ROM_PARTS.md](p4l-rom-parts/ROM_PARTS.md), [design](p4l-rom-parts/ROM_PARTS_DESIGN.md), [plan](p4l-rom-parts/ROM_PARTS_PLAN.md), [MESSAGE.md](machines/080-message/MESSAGE.md) |
| P4m | Message display Puzzles (080) | done (2026-11-11, checked by the user): no new code; six Puzzles, each on a different link of the chain: Half letters (`R3` stuck at 0: the tops of the letters twice), Echo (`U12` input `V8` stuck at 0: the first line again under the poem), Gibberish (`CH5` cracked between the ROMs: letters read as digits and punctuation), Stammer (dry Joint on the message ROM's `A0`: every other letter doubled), Thin letters (bridge from `F1` to a GND tie at `U16`: no right-hand column; a `U15` swap fails), Block letters (`CLK_N` cracked at `U16`: letters smeared into bars); each test finds the faulty page, built from the ROM files, on the TV. [MESSAGE.md](machines/080-message/MESSAGE.md) (Puzzles), [MESSAGE_DESIGN.md](machines/080-message/MESSAGE_DESIGN.md) (Puzzles) |
| P4n | RAM and bus (for 090) | done (2026-11-11, user check pending): `PinKind::Bidirectional` (a Driver and a reader, released at power-on; stuck: forces its Net and is read; marked as a Driver); 2114 1K × 4 static RAM (MOS, Intel maximums, random contents); 74LS244 octal tri-state buffer; DIP-18, DIP-20 (grid packages in `bw place`) and KK-10; `KEYBOARD` connector (`D0`–`D6`, a `STROBE` pulse of `strobe=`, upper case, keys queued, Powered from the Board); typing through the Session, `--type REF:START:TEXT` in `bw run` and `bw_ui`, a Keyboard section with Typing (F2, Esc) in `bw_ui`; bench netlist `tests/data/ram_bench.bwn`. [RAM_BUS.md](p4n-ram-bus/RAM_BUS.md), [design](p4n-ram-bus/RAM_BUS_DESIGN.md), [plan](p4n-ram-bus/RAM_BUS_PLAN.md) |
| P4o | Keyboard window | done (2026-11-11, checked by the user): the ASCII keyboard drawn as a vintage keyboard in a floating window: bit-paired black caps with legends, pressed and released as typed (PC keys, mouse, `--type`); typing while the window is focused, F2 toggles; CTRL and ESC sent (`Session::typeCode`, codes 0x00–0x5F); the keys sent in Debug; legends in Roboto Medium embedded at build. [KEYBOARD_WINDOW.md](p4o-keyboard-window/KEYBOARD_WINDOW.md), [design](p4o-keyboard-window/KEYBOARD_WINDOW_DESIGN.md), [plan](p4o-keyboard-window/KEYBOARD_WINDOW_PLAN.md) |
| P4p | TV typewriter (090), RAM version | done up to the Board (2026-11-12): no new code; 32 × 16 characters at 4 MHz (080's video chain, 42 characters a line, the line counter on a re-timed `LINE_END`), the page in two 2114s behind a 74LS157 address multiplexer, a key written at the cursor during the right-hand border (`KEYBOARD strobe=100us`), Return, wrap, Clear (spaces from a second 74LS244), a blinking block cursor; netlist in three tested stages, manuals; 0.95× headless on a full page, 1.31× after Clear (below real time accepted by the user). Board placed, not routed: superseded by P4q. [TV_TYPEWRITER.md](p4p-tv-typewriter/TV_TYPEWRITER.md), [design](p4p-tv-typewriter/TV_TYPEWRITER_DESIGN.md), [plan](p4p-tv-typewriter/TV_TYPEWRITER_PLAN.md), [TYPEWRITER.md](machines/090-typewriter/TYPEWRITER.md) |
| P4q | Shift-register TV typewriter (090) | done (2026-11-12, user check pending): the page in MOS shift registers, as in the 1973 original (memory without addresses). New Parts from the 1972 Signetics MOS data book: the 2524 512-bit dynamic recirculating shift register (two clocks; a clock stopped 10 ms fades every bit to the level of the last clock phase; DIP-8) and the 2518 hex 32-bit static shift register (the row buffer); no `GND` pin, negative supplies not connected, ordinary outputs. The 090 memory side redone: six 2524s clocked 32 times on the first line of each text row (once round a frame), the 2518 round once a line into the 2513, a key written when the cursor's cell comes round (`KEYBOARD strobe=25ms`), Clear by a frame of spaces, P4p's cursor kept; 40 chips, netlist in two tested stages, manuals, Board placed and routed. Release headless 0.97× random page, 1.21× after Clear (P4p 0.95×, 1.25×); `bw_ui` 1.00× with every instrument. The 2114 and 74LS244 stay for the CPU Machines. [SHIFT_TYPEWRITER.md](p4q-shift-typewriter/SHIFT_TYPEWRITER.md), [design](p4q-shift-typewriter/SHIFT_TYPEWRITER_DESIGN.md), [plan](p4q-shift-typewriter/SHIFT_TYPEWRITER_PLAN.md), [TYPEWRITER.md](machines/090-typewriter/TYPEWRITER.md) |
| P4r | TV typewriter Puzzles (090) | done (2026-11-12, user check pending): no new code; six Puzzles, each a different Fault on a different link: Lookalikes (`PHI2_N` cracked at `U18`: bit 2 fades to 0, `HELLO` as `HAHHK`; a `U18` swap fails), Wallpaper (`WRITE` stuck at 1: each key fills the screen), Copies (`U22` `REC` stuck at 1: every row the same), Every other line (dry Joint on `Y0` at the comparator: rows typed in pairs), Zeros (bridge from `P4` to `READ`'s VCC tie at `U20`: Clear leaves zeros; a `U20` swap fails), M is Return (`CTL` cracked at `U30`: `M` and `-` end the line); each test presses Clear, types, and checks the page or the TV, then the repair. [TYPEWRITER.md](machines/090-typewriter/TYPEWRITER.md) (Puzzles), [TYPEWRITER_DESIGN.md](machines/090-typewriter/TYPEWRITER_DESIGN.md) (Puzzles) |
| P5 | Computers | BW-1, BW-2001, BW-80 (GAME_DESIGN.md, Computers), in that order: the 6502 and the 080/090 video chain reused before the Z80. Buses stay individual Nets, one per line, with their own copper and Faults (decided 2026-11-12): a fault on one line (A4 stuck, D3 cracked, A7–A8 bridged) must emerge without special code. If buses are too slow, the kernel gets faster inside (OPTIMISATION_IDEAS.md), invisible to Parts, Board and Faults. Choices per step (chips, I/O, assembler, key matrix model) asked at its start |
| P5a | Bus load spike | done (2026-11-12): `Z80STUB`, a benchmark Part on the Z80 pinout playing a fixed ZX80 frame (312 lines of 207 T-states, display fetches with refresh, HALT, interrupt acknowledge, ROM and RAM cycles, VSYNC by `IN`/`OUT`) with Z80A pin timing at 3.25 MHz; `netlists/bus_load.bwn`, 26 catalogue chips around it (ROM, 2 × 2114, NOP forced through 74LS244s, character latch, refresh-multiplexed ROM address, 74LS165 at 6.5 MHz), a page of text on the TV; DIP-40 package. Measured: **0.22×** headless, 120 applied events per simulated µs (20× the test pattern), 644 instructions each; the event queue 37 % of the time, the run loop 25 %, Parts 30 %. Decision: kernel work as its own step (P5b) before the 6502. [BUS_LOAD.md](p5a-bus-load/BUS_LOAD.md), [design](p5a-bus-load/BUS_LOAD_DESIGN.md), [plan](p5a-bus-load/BUS_LOAD_PLAN.md) |
| P5b | Kernel for buses | done (2026-11-13, user check pending): `bus_load` 0.225× → **0.40×** headless (643 → 424 instructions and 165 → 90 cycles an applied event), every Net trace identical to the P5a kernel's (`bench/trace_check.sh`), every other netlist faster (test pattern 5.46 → 7.35×, typewriter 1.00 → 1.37×, tennis 1.53 → 1.94×). Kept: the event queue as runs of same-time events, LTO in the release build, specialised evaluation of the ROM and gate packages (ADD_PART), sampled inputs (`PinSpec::sampled`, the `D`s of the 74LS74 and 74LS175), the Net of each Pin kept by the Simulator. Dropped, logged: timing wheels, other heaps, run-loop inlining, the hand-written 74LS157 and 74LS244. The 2× aim is not met within the guards; of the options left (spec, Results), the user chose to go on to the 6502 with this kernel (2026-11-13): the BW-2001 load is measured when its netlist exists (P5k), the real Z80 at P5l; a multi-Pin `EvalContext`, a Bus as one entity, two cores and generated code stay open for then. [KERNEL_BUSES.md](p5b-kernel-buses/KERNEL_BUSES.md), [design](p5b-kernel-buses/KERNEL_BUSES_DESIGN.md), [plan](p5b-kernel-buses/KERNEL_BUSES_PLAN.md) |
| P5c | 6502 Part | done (2026-11-13, user check pending): the NMOS 6502 as a catalogue Part. `Mos6502Core`, apart from the Pins and from the simulator: all 256 opcodes (documented, undocumented, unstable as Tom Harte's data records them, JAM), each cycle's bus as on the chip, NMOS decimal mode, IRQ/NMI polling with the taken-branch delay and NMI taking over BRK, reset. Checked against all 2.56 M cases of Harte's SingleStepTests and Klaus Dormann's functional test (a 100-case-an-opcode subset in git; the full sets fetched into git-ignored `external/6502/` by `tests/data/6502/fetch.sh`, skipped without it). The `6502` Part on the MOS DIP-40 pinout, woken only by `PHI0`, 1 MHz data sheet delays, `RDY` stalling reads only, random power-on state; `GND2` for its second ground. Machine 100 Heartbeat: a free-running 6502 on a NOP bus, its address buffered to 16 LEDs, a Reset button; netlist, manuals, Board placed by `bw place`, tidied and routed by the user; 7.1× headless, 1.00× in `bw_ui` with every instrument; Puzzles in their own row. [MOS6502.md](p5c-6502/MOS6502.md), [design](p5c-6502/MOS6502_DESIGN.md), [plan](p5c-6502/MOS6502_PLAN.md), [HEARTBEAT.md](machines/100-heartbeat/HEARTBEAT.md) |
| P5d | ROM programs + first computer | done (2026-11-13, user check pending): `bw asm <file.asm> -o <file.rom>`, our own two-pass assembler for the 151 documented 6502 opcodes (labels, local labels, constants, C-precedence expressions, `.rom`/`.org`/`.byte`/`.word`), its encoding taken from the core's own table (`Mos6502Decode`); the `.rom` carries the source as comments and a test and the `asm_<m>` ctest keep it equal to its `.asm`. Parts: 74LS138 decoder, 74LS273 octal latch with clear, `POR` power-on reset (open collector, `time=`). Machine 110 LED computer: a 6502 at 1 MHz, a 2716 with the program, two 2114s, a 74LS138 (RAM `$0000`, LEDs `$8000`, ROM `$E000`, mirrored), a 74LS02 making the write strobes, a 74LS273 driving 8 LEDs, the POR and a Reset button; a light bouncing every 124.78 ms, its state and the `JSR` stack in RAM. Netlist, program, manuals, Board placed by `bw place` and routed by the user; 3.8× headless, 1.00× in `bw_ui` with every instrument; Puzzles in their own row. [ROM_PROGRAMS.md](p5d-rom-programs/ROM_PROGRAMS.md), [design](p5d-rom-programs/ROM_PROGRAMS_DESIGN.md), [plan](p5d-rom-programs/ROM_PROGRAMS_PLAN.md), [LED_COMPUTER.md](machines/110-led-computer/LED_COMPUTER.md) |
| P5e | Instruments for buses | done (2026-11-13, user check pending): logic analyzer of 32 Channels × 65 536 points (6.5 ms at 10 MHz), Triggers as bit masks; Channel Groups: shift-click a pin clips its whole bus on the chip (`A0`–`A15` by name stem), hand grouping in the window, a lane in hex, a bus Trigger as a pattern with `x` digits (`E0xx`) ANDed with the other Channels; the window rebuilt on one canvas (input before drawing, the wheel owned: P3c's navigation bugs), drawn from runs, `Fit`/`Trigger`, the value under the cursor; scope of 4 Hooks with a Trigger delay up to 1000 divisions; watch budget 96 (rings not written until used, the Dispatcher merging with a heap); refusals shown at the cursor, tooltips and empty-window hints; the probe and meter read a pin with no copper; `bw_ui --group`, `--delay`. 110 with 32 Clips, 4 Hooks, probe and meter: 1.00×, UI 0.6 ms. [BUS_INSTRUMENTS.md](p5e-bus-instruments/BUS_INSTRUMENTS.md), [design](p5e-bus-instruments/BUS_INSTRUMENTS_DESIGN.md), [plan](p5e-bus-instruments/BUS_INSTRUMENTS_PLAN.md) |
| P5f | Instrument polish | done (2026-11-14, user check pending): the scope and the logic analyzer on one view model, sorted from an [inventory](p5f-instrument-polish/INVENTORY.md) of both windows: 10 divisions of a 1-2-5 time per division, shown with `-`/`+` and zoomed by the wheel about the mouse, panned by a drag or a trackpad swipe; a grid anchored to the Trigger with times from it on an axis strip above the waveforms; the orange Trigger mark in both, an arrow with its distance when off screen; a marker tagged `M` (click, right-click or `Esc` to clear) and one status line `cursor · M · dt (frequency)`. Analyzer: `Sample every 100 ns · 6.55 ms captured`, `Before T` as a time, an overview of the record as a scrollbar, first view at 10 points per division about T (bus values readable), `Fit`, `T`, bus values cut by the view's edge drawn open, groups named `U1 A[15:0]`. Scope: the view is its settings (wheel steps the timebase, drag moves the position from the press), the stored Sweep redrawn at once, bands only for Hooks sharing the height, level and volts at the cursor in the Hook rows, `1 V/div`, `Stop`/`Run`. Shared maths in `src/view/time_view` (unit tested); `bw_ui --sample`, `--view`, `--marker`, `--cursor`. 110 with every instrument: 1.00×, UI 0.6 ms. [INSTRUMENT_POLISH.md](p5f-instrument-polish/INSTRUMENT_POLISH.md), [design](p5f-instrument-polish/INSTRUMENT_POLISH_DESIGN.md), [plan](p5f-instrument-polish/INSTRUMENT_POLISH_PLAN.md) |
| P5g | Memory faults | a bad bit in a RAM or ROM; wrong IC fitted |
| P5h | Computer Puzzles | Puzzles for the 100 heartbeat and the 110 LED computer, found with the bus instruments: the candidates in [HEARTBEAT.md](machines/100-heartbeat/HEARTBEAT.md) and [LED_COMPUTER.md](machines/110-led-computer/LED_COMPUTER.md) (a data or address line cracked or pulled, `RES_N` or the power-on reset stuck, a dead clock, the ROM's `CE_N` stuck, a decoder select bridged, `LATCH_CLK` cracked, `WR_N` stuck high) and P5g's memory faults (a bad RAM bit that breaks the stack, a wrong IC fitted) |
| P5i | Key matrix | key matrices scanned by the CPU: one generic model, a layout per Machine, on the Board (keypad) or behind a Connector (keyboard) |
| P5j | BW-1 (KIM-1) | 6502, own monitor ROM, RAM, I/O, multiplexed LED digits driven by the program, hex keypad; Puzzles in their own row |
| P5k | BW-2001 (PET 2001) | 6502, the 080/090 video chain on video RAM, CPU/video arbitration, keyboard, screen-editor monitor; netlist in tested stages, each stage's ratio measured (headless and `bw_ui`): below 1× goes to the user with P5b's options; Puzzles in their own row |
| P5l | Z80 Part | exact at the Pins: M1, refresh, HALT, interrupt acknowledge, I/O; checked against ZEXDOC and Tom Harte's Z80 tests; measured again on `netlists/bus_load.bwn` in place of the stub |
| P5m | BW-80 (ZX80) | NOPs forced through a resistor-isolated data bus, ROM address multiplexed by refresh, sync from CPU cycles; schematic reconsidered (Board authoring); Puzzles in their own row |
| Later | Timing | setup/hold checks, slow and wrong parts, timing faults |

P1–P3 test the idea, with hand-placed faults only. P1 (a–c): is the kernel fast enough? P2: can we produce boards? Do symptoms emerge without scripting, and can a player diagnose them? P3: is the full loop fun?

## Board authoring

KiCad is not the source of truth: its scope is real-world manufacturing, ours is "works in Bodgewire". At most it is a layout editor. Own pipeline, two separate steps:

- **Netlist** (authored, text): parts from our own chip library (only parts we model) + named nets listing pins. Own compact line format (`part U1 74LS161`, `net CLK X1.OUT U1.2 U2.2`), own parser with electronics-aware errors (unknown part, bad pin, pin on two nets). Validated headlessly with our tools (does it produce sync, pass its tests?) before any layout exists. This is the design netlist.
- **Layout**: KiCad PCB editor as layout tool only (decided in P2a, 2026-10-27). `bw place` writes the placed Board from the netlist and a rules file (P4k, 2026-11-11), the user routes it in pcbnew (ratsnest, DRC, interactive router or Freerouting); the `.kicad_pcb` is read back (footprint positions, track segments, vias). No schematic editor, no circuit editing. Projects in `boards/<name>/`; steps in [KICAD_GUIDE.md](p2a-board-layout/KICAD_GUIDE.md).
  - P2a result: counter and test-pattern Boards laid out by a KiCad newcomer (KiCad 10, test pattern auto-routed by Freerouting: 479 segments, 13 vias), both pass `bw check-board`. Pain points were one-offs or guide material: library table path after install, Java for Freerouting, ratsnest redrawn differently while moving, outline drawn on F.Cu by mistake, one connection left by Freerouting.
  - Next: everything after the hand layout is automated (export, read, check; KiCad's DRC headless via `kicad-cli pcb drc` in the tests). Boards need connectors as Parts in the netlist: power entry (VCC, GND) and outputs (video), so they get footprints, pads and Traces like any chip.
- Whatever the layout source, Bodgewire checks the copper connects exactly what the netlist says.
- No schematic for now (decided 2026-10-28): on small Boards, Net highlighting with Driver marks, the Chip pinout and the datasheets answer "what feeds what"; a generated net-label schematic would only repeat the pinouts, and a wired one costs hand work per Machine. Revisit when Machines outgrow that (BW-80, ~40 chips).
- Layout quality is a visual problem only; improvable later without touching simulation.
- Later (asked by the user, 2026-11-13, after tidying the 100 Board by hand): a hand-tidied placement survives a regeneration. Today `bw place` refuses a routed Board without `--force`, and with it starts again from the `.place` rules, so moved and rotated parts are lost when the netlist changes. Candidates: `bw place` keeps the position and rotation of every part already on the Board and places only new ones, or reads the positions back into the `.place` as fixed rules.
- Risk: dense boards the router can't finish (BW-80 on 2 layers). Fallbacks: extra layer, wire jumpers (common on period boards).
