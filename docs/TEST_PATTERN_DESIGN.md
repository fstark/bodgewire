# Test-pattern Generator — Detailed Design (P1c)

Implements the specification in [TEST_PATTERN.md](TEST_PATTERN.md) (sections S1–S4).

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md). Terminology: [GLOSSARY.md](GLOSSARY.md). Uses the P1b Parts and netlist format ([PARTS_DESIGN.md](PARTS_DESIGN.md)); no new library code.

---

## 1. Directory Layout

```
netlists/test_pattern.bwn          the Machine (§4)
tests/machines/test_pattern_test.cpp   golden check (§5)
```

No change to `src/`. The benchmark is `bw run` (§6).

## 2. Block Diagram

```
X1 1 MHz ─CLK─┬─ U1 ─ U2  horizontal counter H0–H5 (0..63)
              │      └─ U6 NAND4 ─ LINE_END_N ─ U9 ─ LINE_END
              ├─ U3 ─ U4 ─ U5  vertical counter V0–V8 (0..311), enabled by LINE_END
              │                 └─ U7 NAND8 ─ FRAME_END_N (synchronous load 0)
              │  U6 NAND4 ─ HSYNC_N    U8 NAND8 ─ VSYNC_N    U10 XOR ─ composite sync
              │  U11 AND, U12 OR, U13 NOR ─ blanking and grid ─ video
              └─ U14 74LS74: registers SYNC_N and VIDEO
```

## 3. Decoding

All decodes are partial: they rely on the counter never exceeding its range once locked.

| Signal | Condition | Logic | Why partial decode is safe |
|---|---|---|---|
| `HC` | H0–H3 = 15 | `U1.RCO` | — |
| `LINE_END` | H = 63 | `NAND(HC, H4, H5, 1)` inverted | H6/H7 (U2.QC/QD) ignored; U2 is loaded with 0 at every line end |
| `FRAME_END_N` | V = 311 and H = 63 | `NAND(V8, V5, V4, V2, V1, V0, LINE_END, 1)` | 311 is the smallest value with these bits set; V6, V7 are 0 below 311 |
| `HSYNC_N` | H = 52–55 | `NAND(H5, H4, H3_N, H2)` | `110100`–`110111` |
| `VSYNC_N` | V = 308–311 | `NAND(V8, V5, V4, V2, V3_N, 1, 1, 1)` | the only values ≤ 311 with these bits |
| `HBLANK` | H ≥ 48 | `AND(H5, H4)` | |
| `VBLANK` | V ≥ 288 | `AND(V8, V5)` | 288 = 256 + 32, max 311 |
| `ACTIVE` | neither blank | `NOR(HBLANK, VBLANK)` | |
| `GRIDH` | H mod 8 = 0 | `NOR(OR(H2, H1), H0)` | |
| `GRIDV` | V mod 16 = 0 | `NOR(OR(V3, V2), OR(V1, V0))` | |
| `SYNC` | composite, active high | `XOR(HSYNC_N, VSYNC_N)` | = HSYNC outside vertical sync, inverted inside |
| `SYNC_N_D`, `VIDEO_D` | register inputs | `XOR(SYNC, 1)`, `AND(ACTIVE, GRID)` | |

Lock from a random power-on state: the horizontal counter within one line (64 µs); the vertical counter at the next match of the `FRAME_END_N` bit pattern, which recurs every 512 lines (≤ 32.8 ms).

Timing: the longest path after a clock edge is the video decode, CLK→QA (15 ns) → U12 OR (14 ns) → U13 NOR (10 ns) → U12 OR (14 ns) → U11 AND (10 ns) ≈ 63 ns; the RCO chain into U5.ENT is ≈ 58 ns. Both are far inside the 1 µs pixel.

## 4. Netlist — `netlists/test_pattern.bwn`

```
# Test-pattern generator (TEST_PATTERN.md): 1 MHz dot clock, 64 pixels x 312 lines,
# PAL-like progressive. Outputs: SYNC_N (composite sync) and VIDEO (grid), registered.

part X1  OSC freq=1MHz
part U1  74LS161    # H0-H3
part U2  74LS161    # H4-H5
part U3  74LS161    # V0-V3
part U4  74LS161    # V4-V7
part U5  74LS161    # V8
part U6  74LS20     # LINE_END_N, HSYNC_N
part U7  74LS30     # FRAME_END_N
part U8  74LS30     # VSYNC_N
part U9  74LS04     # H3_N, V3_N, LINE_END
part U10 74LS86     # composite sync
part U11 74LS08     # blanking, video gate
part U12 74LS32     # grid ORs
part U13 74LS02     # grid NORs, ACTIVE
part U14 74LS74     # output registers

net CLK         X1.OUT U1.CLK U2.CLK U3.CLK U4.CLK U5.CLK U14.1CLK U14.2CLK

# Horizontal counter 0..63: U1 free-running, U2 loaded with 0 at each line end.
net VCC         U1.CLR_N U1.ENP U1.ENT U1.LOAD_N U2.CLR_N U2.ENP
net GND         U1.A U1.B U1.C U1.D U2.A U2.B U2.C U2.D
net H0          U1.QA U13.2B
net H1          U1.QB U12.1B
net H2          U1.QC U12.1A U6.2D
net H3          U1.QD U9.1A
net HC          U1.RCO U2.ENT U6.1A
net H4          U2.QA U6.1B U6.2B U11.1B
net H5          U2.QB U6.1C U6.2A U11.1A
net VCC         U6.1D
net LINE_END_N  U6.1Y U2.LOAD_N U9.3A
net LINE_END    U9.3Y U3.ENP U3.ENT U4.ENP U5.ENP U7.G
net H3_N        U9.1Y U6.2C
net HSYNC_N     U6.2Y U10.1A

# Vertical counter 0..311: counts at each line end, loaded with 0 at the frame end.
net VCC         U3.CLR_N U4.CLR_N U5.CLR_N U7.H
net GND         U3.A U3.B U3.C U3.D U4.A U4.B U4.C U4.D U5.A U5.B U5.C U5.D
net V0          U3.QA U7.F U12.3B
net V1          U3.QB U7.E U12.3A
net V2          U3.QC U7.D U8.D U12.2B
net V3          U3.QD U9.2A U12.2A
net VC1         U3.RCO U4.ENT
net V4          U4.QA U7.C U8.C
net V5          U4.QB U7.B U8.B U11.2B
net V6          U4.QC
net V7          U4.QD
net VC2         U4.RCO U5.ENT
net V8          U5.QA U7.A U8.A U11.2A
net FRAME_END_N U7.Y U3.LOAD_N U4.LOAD_N U5.LOAD_N
net V3_N        U9.2Y U8.E
net VCC         U8.F U8.G U8.H
net VSYNC_N     U8.Y U10.1B

# Composite sync: HSYNC XOR VSYNC, then inverted.
net SYNC        U10.1Y U10.2A
net VCC         U10.2B
net SYNC_N_D    U10.2Y U14.1D

# Video: grid inside the active picture.
net HBLANK      U11.1Y U13.1A
net VBLANK      U11.2Y U13.1B
net ACTIVE      U13.1Y U11.3A
net H21         U12.1Y U13.2A
net GRIDH       U13.2Y U12.4A
net V32         U12.2Y U13.3A
net V10         U12.3Y U13.3B
net GRIDV       U13.3Y U12.4B
net GRID        U12.4Y U11.3B
net VIDEO_D     U11.3Y U14.2D

# Output registers.
net SYNC_N      U14.1Q
net VIDEO       U14.2Q
net VCC         U14.1PRE_N U14.1CLR_N U14.2PRE_N U14.2CLR_N

# Unused gate inputs.
net GND         U9.4A U9.5A U9.6A U10.3A U10.3B U10.4A U10.4B U11.4A U11.4B U13.4A U13.4B
```

14 chips + oscillator. Every input is connected (no warnings).

## 5. Golden Check

In `tests/machines/test_pattern_test.cpp`, file-local:

```cpp
constexpr int kPixelsPerLine = 64, kLinesPerFrame = 312;
constexpr int kPixelsPerFrame = kPixelsPerLine * kLinesPerFrame;  // 19'968
constexpr Time kPixel = 1'000'000;  // ps

struct Outputs { bool syncN; bool video; };

// S1/S2 decode of counter state (h, v), before the output register.
constexpr Outputs Decode(int h, int v) {
    const bool hsync = h >= 52 && h <= 55;
    const bool vsync = v >= 308 && v <= 311;
    const bool active = h < 48 && v < 288;
    return {hsync == vsync, active && (h % 8 == 0 || v % 16 == 0)};
}

// Registered output while the counter shows index n (n = v * 64 + h): Decode of n - 1.
constexpr Outputs Expected(int n);
```

Algorithm:

```
load test_pattern.bwn (no warnings); Simulator sim; powerOn(seed); record SYNC_N, VIDEO
runFor(50 ms + 4 frames)
T = time of the first SYNC_N falling edge after 50 ms that starts a low interval ≥ 32 µs
    // = start of counter index 308 * 64 + 1 (first pixel showing vertical sync), plus CLK→Q
n0 = 308 * 64 + 1
for k in 0 .. 3 * kPixelsPerFrame - 1:
    t = T + k * kPixel + kPixel / 2                  // centre of the pixel
    if ValueAt(SYNC_N, t) != Expected(n0 + k).syncN: ++mismatches   // report first 10
    if ValueAt(VIDEO,  t) != Expected(n0 + k).video: ++mismatches
REQUIRE(mismatches == 0)
CHECK(transitions of SYNC_N in [T, T + 3 frames) == transitions of Expected().syncN)
same for VIDEO
```

`ValueAt(samples, t)`: last sample with `time ≤ t` (binary search). Mismatches are reported as `frame f line v pixel h: SYNC_N expected 1 got 0`, first 10 only (counting instead of a `CHECK` per sample avoids 120 k `CHECK` lines).

Tests:

| Name | Covers |
|---|---|
| `"test pattern: loads without warnings"` | S3 |
| `"test pattern: outputs match golden function"` | S3, seeds 1, 2, 3 (`GENERATE`) |
| `"test pattern: decode sanity"` | `Decode` itself: 4 sync pixels per normal line, 60 per vertical-sync line; 2'484 white pixels per frame (6 columns × 288 + 18 lines × 48 − 108 crossings) |

## 6. Benchmark

```
cmake --preset release && cmake --build --preset release
./build/release/src/cli/bw run netlists/test_pattern.bwn --time 1s
```

Pass: `ratio ≥ 10`. The plan records `ratio`, `applied_events` and `evaluations`. If below 10, the plan marks the phase **Skipped: below target** with the numbers, and a P1d kernel-performance milestone (profile first) is opened.

## 7. Build Integration

- `tests/CMakeLists.txt`: add `machines/test_pattern_test.cpp`. `BW_NETLIST_DIR` already exists (P1b).

## 8. Reused Infrastructure

| Existing | Used for |
|---|---|
| Parts 74LS02/04/08/20/30/32/74/86/161, OSC | the whole circuit |
| `LoadNetlistFile`, `LoadedCircuit::nets` | test, CLI |
| `Simulator::record`, `Recorder::samples` | golden check |
| `bw run` | benchmark and VCD |
