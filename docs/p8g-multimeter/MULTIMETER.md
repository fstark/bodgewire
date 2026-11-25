# P8g — The Multimeter held

Roadmap step P8g (ARCHITECTURE.md#roadmap): the Multimeter stops being a floating window. It is held like the Logic Probe; while the player presses on copper with it, its LCD rises just above its yellow drawing on the Shelf, and goes when the press ends. The power switch is its mode. Design: [MULTIMETER_DESIGN.md](MULTIMETER_DESIGN.md). Plan: [MULTIMETER_PLAN.md](MULTIMETER_PLAN.md).

## Why

The user, 2026-11-25, on the meter's window (`src/ui/meter_window.cpp`):

- it is huge, about 20 % of it useful (a mode choice, one readout, two lines naming the leads);
- it looks nothing like a multimeter;
- after each measure the window has to be closed;
- continuity is hidden behind a mode click: a 2-in-1 instrument;
- it works, but feels unnatural.

A real meter is used by holding its tip on a pin and reading the display, then lifting it. The step makes the game's meter work the same way.

## User decisions

From the roadmap row and a grilling (the user, 2026-11-25):

- The meter stays on the Shelf, a held tool like the Logic Probe; no window, no bench place, no presets, no pop-out.
- Its LCD appears just above the yellow meter on the Shelf, only while it measures; nothing stays attached.
- The power switch is the mode: Volts while the Machine is powered (black on GND, implied), Continuity while it is off. No mode button, no "power off for continuity" refusal.
- Volts: press on copper and hold; the LCD shows the reading; sliding reads each copper passed; release ends it.
- Continuity: a click clips the black lead, which stays; press-and-hold then probes with the red lead; a click on the clip lifts it; the power going on lifts it too. (A drag from one point to the other was proposed and rejected: a complex gesture, and fixing one point then poking around is the real use.)
- The LCD: the top of the yellow handheld, a grey LCD with seven-segment digits; `2.43 V`, dashes while settling; in Continuity `OL` open, `0.0` with the `•)))` sign and a green backlight when connected (no sound in `bw_ui`). No dial, no lead labels: the Board's + and − tags show where the leads are.
- Paused (the run switch at `‖`): no reading, dashes. A paused instant-level reading was proposed and dropped: "In real life you cannot read a paused board."
- A pin link in a manual or the chip panel held down with the meter in hand measures that pin until released, as a press on it does.
- Touch: a finger held brings up the loupe, the LCD above the Shelf's meter, reading under the loupe live; lifting ends it.
- The Logic Probe is out of scope (the user may want it changed later; it is watched over time).

Chosen without asking:

- A pin link is the pin: held, the red lead on it; clicked, a click on it. In Continuity a click on a link with no clip clips the black lead there, as a click on the pad does.
- With the meter, a left drag that starts on copper reads instead of panning (as the Wire tool's drag draws); one that starts off copper pans, and the right or middle button still pans (a trackpad has no middle button). A finger's drag still pans: the finger reads through the loupe.
- While paused the LCD shows dashes whatever it read before the pause, a rail included.
- While pressed off copper the LCD stays up: dashes in Volts, `OL` in Continuity (a real meter's tip in the air), so sliding between pins does not flicker.
- In Continuity with no clip, a press shows no LCD (nothing to compare with): its release is the click that clips. A press that starts on the clip shows none either: its release lifts it.
- The Shelf's meter drawing loses its fake `5.0`: held, its own small LCD is lit and blank.
- The Shelf's hint names the next step with the meter in hand (§4).
- `--meter A` holds the red lead on A from the start (the LCD up, for screenshots), until the first press; `--meter A,B` (the power left off) clips black on A and holds red on B. The meter is in hand.

## What the player sees

1. **The meter in hand.** A click on the Shelf's meter takes it, framed in gold, as the Logic Probe; taking another tool puts it down. It has no window; the click no longer brings one out, and a double-click pops nothing out. Its Shelf tooltip: "Multimeter: hold on copper; volts, or continuity with the power off", plus "black clip: U1.14 QA leg" while clipped.
2. **The power is the mode.** Powered: Volts. Off: Continuity. The Shelf's power switch is the only mode control.
3. **Volts.** Press on copper and hold: the LCD rises above the Shelf's meter (§5) and reads the Net's average volts (200 ms, refreshed every 250 ms of simulated time, as before; dashes until the first reading). Slide, still pressed: each copper passed is read afresh. Off copper: dashes. Release: the LCD goes. The Board does not pan under that drag. Paused: dashes.
4. **Continuity.** With the power off:
   - no clip: a click on copper clips the black lead there; the Board's − tag marks it. Hint: "Meter: click to clip the black lead".
   - clipped: press and hold anywhere else: the LCD shows `0.0`, lit green with `•)))`, when the red lead's copper is connected to the clip's, `OL` when not (also off copper). Hint: "Meter: hold the red lead on copper; click the clip to lift it".
   - a click on the clip (the copper it is on) lifts it. Powering on lifts it too.
   - A Repair keeps the clip; the next press reads the repaired Copper.
   With the power on and the meter in hand the hint says "Meter: hold on copper to read volts".
5. **The LCD.** About 150 × 64 px, its bottom on the Shelf's top edge, centred over the meter's drawing (inside the window): the yellow case's top, a grey-green LCD, three seven-segment digit cells with their decimal points, the unused segments faintly visible; `V` in Volts, the `•)))` sign in Continuity. Drawn above every window.
6. **Pin links.** With the meter in hand, a pin link in a manual or the chip panel held down measures that pin (§3, §4: the LCD up, the pin flashed) until released; clicked, it is a click on the pin (in Continuity, the clip). Cmd-click and a double-click's second frame the pin, as before.
7. **Touch.** A finger's drag pans, as before. A finger held still brings up the loupe; from then the meter reads the copper under the loupe's centre, the LCD up, until the finger lifts. A tap is a click (in Continuity, the clip).
8. **Flags.** `--meter REF.PIN[,REF.PIN]` as above; `--focus meter` takes it; `--pop meter` and `--away meter` are refused (no window).

## What is new

1. **Session**: `MeterSetup` rewritten for the held meter: `mode()` from the power, `hold(red)`, `clip(black)` / `unclip()`; the power going on lifts the clip.
2. **View**: `MeterReading` (what the LCD shows, pure); the meter out of `BenchObject` and no longer an Equipment Instrument in `Focus`; `Measures(owner)` for links and double-clicks.
3. **UI**: the meter's press in `BoardView` (`meterPress()`), held pin links (`TargetHits::held`), the clip in `ToolClick`, the LCD drawing (`ui/meter_lcd`), the Shelf (hint, tooltip, anchor), `main.cpp`; `meter_window.{hpp,cpp}` deleted.

Untouched: the kernel, the runner, the meter's engine and Consumer, Parts, Machines, Puzzles, the Logic Probe's look, the Analyzer and the Oscilloscope, `bw` and the headless `Multimeter`.

## Tests

- Unit: `meter reading: …` (volts, dashes, rounding, `12.0`, continuity, segments); `focus: …` (the meter taken like the probe), `bench places`, `bench options` updated; `session: meter …` rewritten for `hold`/`clip` (volts, the clip lifted by power on, continuity follows a mend, undo, leads survive a repair); `bodge wire`, `shorts power` updated.
- Screenshots (release, viewed): Volts held on a counter's output (`--meter`), paused (dashes), Continuity connected and open (`--meter A,B`), the Shelf with the meter in hand and no LCD, a touch hold (`--touch --hold`) with the loupe and the LCD.
- Speed: the 110 with the meter held, ratio and UI ms as before (1.00×, under 1 ms).
- Debug (ASan + UBSan): the meter's, focus and session cases, without the BW-2001, BW-80 and tennis ones.
- The full release suite, the web build.
- Only the user can check: the feel of holding, the LCD's look at real size, touch on a tablet.

## Later

- The Logic Probe held the same way (the user is unsure: it is watched over time).
- A beep through the speakers.
