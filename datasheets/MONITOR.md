# MONITOR — monitor connector, separate drives

## Function

The 7-pin connector to a monitor that takes its sync as two separate signals, as the PET 2001's built-in monitor does (the PET's `J7`). The monitor reads three logic signals:

- `VIDEO_N`: the picture, **low** for a lit dot (the PET's TTL video is inverted);
- `HDRIVE`: horizontal drive; a line starts at each rise;
- `VDRIVE`: vertical drive; a frame starts at each fall.

As with the composite `VIDEO` connector, the monitor's own oscillators run slightly slow and are pulled into step by drive edges that arrive late enough: without them the picture rolls or tears.

## Pins

- `VIDEO_N`: picture, low for a lit dot (pad 1)
- `GND1`: ground (pad 2)
- `VDRIVE`: vertical drive (pad 3)
- `GND2`: ground (pad 4)
- `HDRIVE`: horizontal drive (pad 5)
- `NC6`: not connected (pad 6)
- `GND3`: ground (pad 7)
