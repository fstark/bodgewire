# PADDLE — paddle controller connector

## Function

The 4-pin connector of a paddle controller: a small box with a knob, on a cable. Inside, the knob turns a potentiometer that sets the time of a 555 timer wired as a one-shot. Each fall of `TRIG_N` makes `OUT` go high for a time set by the knob: shortest at the left stop, longest at the right, in proportion between. The shortest and longest times are given in the Machine's Service Manual. A controller may be wired the other way round (the Machine's Service Manual says so): the longest time at the left stop and the shortest at the right. A new fall of `TRIG_N` while `OUT` is high is ignored; while idle, `OUT` is low.

The controller runs on the Board's +5 V through `VCC` and `GND`. Without it, `OUT` drives nothing.

## Pins

- `TRIG_N`: trigger from the Board; each fall starts a pulse
- `OUT`: the pulse to the Board, high while it lasts
- `VCC`: +5 V for the controller, from the Board
- `GND`: 0 V
