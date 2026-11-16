# 010 — Counter

First rung of the tutorial ladder. One blinking LED makes a 1 MHz counter chain something the player can watch; the chain itself is checked with the instruments. Its code came with roadmap step P4a ([COUNTER_LED.md](../../../docs/p4a-counter-led/COUNTER_LED.md)).

## Lesson

A symptom ("the LED does not blink") has several possible causes along one chain. Walk the chain with the probe from the clock up, and find where the signal stops.

## Circuit

- `X1` 1 MHz `OSC` → net `CLK` → clock pins of `U1`–`U5`.
- `U1`–`U5`: five 74LS161 chained into one 20-bit counter, bits `C0`–`C19` (`U1` = `C0`–`C3`, …, `U5` = `C16`–`C19`).
  - All `ENP` and `LOAD_N` high, data inputs at 0 V, `CLR_N` on `RESET_N` pulled up by `R1`.
  - `U1.ENT` high; carries `CARRY1`–`CARRY4`: `U1.RCO` → `U2.ENT`, …, `U4.RCO` → `U5.ENT`.
- `D1` red LED, wired period-correct: +5 V → `R2` → `D1` → `C19`. It lights while `C19` is low.
- `J1` power connector.
- The previous `U3` 74LS74 divider and `U4` 74LS00 decoder are removed.

## When it works

- `CLK` pulses at 1 MHz; each bit `Cn` runs at half the rate of the one below it.
- `C19` completes a cycle every 1.05 s: `D1` is on for about 0.5 s, off for about 0.5 s.
- The low bits are far too fast to see; only the probe, scope or analyzer show them.

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Dark LED | Broken Trace on `C19` between `U5` and `D1` | `D1` never lights | `C19` pulses at `U5` pin 11, floats at `D1`: mend the Trace |
| Frozen LED | `U3.RCO` (`CARRY3`) stuck at 0 | `D1` stays on or off for ever | `CLK`, `C0`–`C11` and `CARRY1`–`CARRY2` pulse; `CARRY3` never does: swap `U3` |
| Too fast | `U1.RCO` (`CARRY1`) stuck at 1 | `D1` flickers at about 15 Hz | `CARRY1` is high all the time instead of a pulse every 16 µs: swap `U1` |

Netlist, Board, Puzzle files and manuals: [COUNTER_DESIGN.md](COUNTER_DESIGN.md).
