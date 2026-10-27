# Test-pattern Generator — Specification (P1c)

Last milestone of P1 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). The first real Machine as a netlist: an oscillator and 74LS chips that produce TV sync and a grid picture, checked automatically, and the P1 performance benchmark run on it. Builds on [KERNEL.md](../p1a-kernel/KERNEL.md) and [PARTS.md](../p1b-parts/PARTS.md). Design: [TEST_PATTERN_DESIGN.md](TEST_PATTERN_DESIGN.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Scope

In:

- `netlists/test_pattern.bwn`: the circuit, using only P1b Parts.
- Automated check of both outputs (composite sync and video) against a golden function, sampled at the centre of each pixel.
- Benchmark: the netlist headless, release build.

Out (P2): TV model, picture on screen, Board layout, Faults.

## Behaviour

### S1 Video standard

PAL-like, progressive (non-interlaced), as on many 8-bit computers. One pixel = one oscillator cycle.

| | Value |
|---|---|
| Oscillator (dot clock) | 1 MHz (1 µs per pixel) |
| Line | 64 pixels = 64 µs (15.625 kHz) |
| Frame | 312 lines = 19.968 ms (≈ 50.08 Hz) |
| Active picture | pixels 0–47, lines 0–287 (48 × 288) |
| Horizontal sync | pixels 52–55 (4 µs), front porch 48–51, back porch 56–63 |
| Vertical sync | lines 308–311 (4 lines), vertical blanking lines 288–311 |
| Composite sync | horizontal XOR vertical (inverted horizontal pulses during vertical sync) |

The dot clock is low on purpose: it keeps the event rate within reach of the benchmark (ARCHITECTURE.md, Performance). 48 pixels per line is coarse but enough for a grid.

### S2 Outputs

- `SYNC_N`: composite sync, active low.
- `VIDEO`: 1 = white, 0 = black; 0 outside the active picture.
- Picture: a grid. White where `pixel mod 8 = 0` (vertical lines) or `line mod 16 = 0` (horizontal lines), inside the active picture.
- Both outputs are registered on the dot clock (one 74LS74), so they are glitch-free and lag the counters by one pixel.
- The circuit has no reset: from any power-on state it locks to the standard frame within 512 lines.

### S3 Checks

- The netlist loads with no errors and no warnings.
- After 50 ms (lock time plus margin), over 3 frames: `SYNC_N` and `VIDEO` sampled at the centre of every pixel equal the golden function of S1/S2; and both change exactly as many times as the golden function predicts (no glitches).
- Holds for several power-on seeds.

### S4 Benchmark

- `bw run netlists/test_pattern.bwn --time 1s` in a release build.
- Pass: ratio (simulated / wall-clock) ≥ 10.
- Expected from the P1a numbers (≈ 26 M events/s, ≈ 5 events per pixel): about 5×. If below 10×, the next milestone is kernel performance (P1d), starting with profiling; P2 does not wait for it.

## Done when

- S3 checks pass in debug (sanitizers) and release.
- `bw run netlists/test_pattern.bwn --time 40ms --record SYNC_N,VIDEO --vcd pattern.vcd` looks right in a viewer.
- S4 measured and recorded in the plan; ≥ 10× or P1d opened.
