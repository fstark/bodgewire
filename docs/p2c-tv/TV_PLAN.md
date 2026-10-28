# TV — Implementation Plan (P2c)

Design: [TV_DESIGN.md](TV_DESIGN.md)
Spec: [TV.md](TV.md)

**P2c done 2026-10-28**, commits 78c6b73 to the "tv: phase 5" commit. All 5 phases completed:

1. `Recorder::trim` / `Simulator::trimRecording`; `ConnectorNet()` moved into the netlist library.
2. `bw_devices` module; `Tv` sync separation, triggered line/frame deflection, lock.
3. Frame buffer painting (X = grey) and `WriteGreyscaleBmp()`.
4. `TvInput` (Simulator → Tv, Recorder trimmed per slice); the test pattern's grid lands exactly where TV_DESIGN §5.5 predicts, for seeds 1–3.
5. `bw run --tv FILE.bmp` (prints `tv_locked`); debug and release green (214 tests); TV cost within noise (~5.2× real time with and without).

Findings:
- Vertical pull-in is slow by design (1 ms per free-running frame): the test pattern locks after 39–314 ms depending on the power-on phase. TV.md S5 updated to "within 0.5 s".
- The top grid line (v = 0) is drawn 4 µs left of the rest, the hook the vertical-sync lines cause on a real set.

Follow-up: none.
