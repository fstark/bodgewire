# VIDEO — TTL monitor connector

## Function

The 3-pin connector to the monitor. The monitor reads two logic signals: `VIDEO` (high is a white dot, low is black) and `SYNC_N` (low pulses tell it where lines and frames start). Without regular sync pulses the picture cannot lock: it rolls or tears.

## Pins

- `VIDEO`: picture signal to the monitor, high is white
- `SYNC_N`: sync to the monitor, active low
- `GND`: 0 V
