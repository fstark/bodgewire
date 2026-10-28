# Counter — Service Manual

## How it works

**Clock.** The crystal oscillator `X1` makes a 1 MHz square wave on its pin 8 (`OUT`), net `CLK`. It goes to the clock pins of both counters.

**Reset.** `R1` pulls `RESET_N` up to +5 V, so the counters are never cleared. Nothing pulls it low on this board.

**Counter.** `U1` and `U2` are two 4-bit counters chained into one 8-bit counter. `U1` counts on every clock (`C0`–`C3`). When `U1` reaches 15 its ripple carry `CARRY` (`U1` pin 15) goes high and lets `U2` count one step on the next clock (`C4`–`C7`). The whole count runs 0 to 255 and wraps every 256 µs.

**Divider.** Flip-flop 1 of `U3` has its inverted output wired back to its data input, so it changes state on every rising edge of `C7`: `DIV` (`U3` pin 5) is `C7` divided by two. Flip-flop 2 is unused.

**Decoder.** NAND gate 1 of `U4` looks at `C6` and `C7`: `TOP_N` (`U4` pin 3) is low while both are high, that is for counts 192 to 255. Gates 2 to 4 are unused, their inputs tied to 0 V.

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `R1` pull-up: holds `RESET_N` high so the counters run
- `U1` low counter: bits `C0`–`C3`, carry to `U2`
- `U2` high counter: bits `C4`–`C7`, counts when `U1` carries
- `U3` divider: `DIV` toggles on each rising `C7`
- `U4` top decoder: `TOP_N` low while `C6` and `C7` are high

## Troubleshooting

- Check `X1` pin 8 (`OUT`): it must pulse. If not, nothing else can move: check power at `X1` pin 14.
- Check `U1` pin 1 (`CLR_N`): it must be high. If low, the counters are held at 0.
- Check `U1` pin 14 (`QA`) to `U1` pin 11 (`QD`): each must pulse. A bit that is stuck high or low points at `U1` or the Trace leaving that pin.
- Check `U1` pin 15 (`RCO`): a short pulse every 16 µs. If missing, `U2` never counts.
- Check `U2` pin 14 (`QA`) to `U2` pin 11 (`QD`): each must pulse, slower than `U1`'s bits.
- Check `U3` pin 5 (`DIV`) and `U4` pin 3 (`TOP_N`): both must pulse. If only one is dead, look at that chip and the Traces into it.
