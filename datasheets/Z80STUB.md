# Z80STUB — Z80 stub playing a ZX80 frame (benchmark)

## Function

A stand-in for a Z80 processor, used to measure how fast a ZX80-like computer can run. It executes no program: on each edge of `CLK` it plays the next step of a fixed list of Z80 bus cycles, with the real chip's pin order and timing at 3.25 MHz. The list is one picture of a ZX80 showing its display: 312 lines of 207 clock cycles. On the 192 text lines the stub fetches 32 characters from the display file, the line's end (a HALT), then answers an interrupt; on the other lines it fetches from the ROM, reads and writes the RAM, and its writes fill the display file with a fixed page. An input from port `FE` starts the vertical sync, an output to it ends it.

The inputs other than `CLK` are not read: interrupts, waits, bus requests and reset change nothing. The data pins are driven only during writes and outputs.

## Pins

- `A0`–`A9`, `A10`–`A15`: address; `A0` is the lowest bit
- `D0`–`D7`: data, driven during writes and outputs, floating otherwise; `D0` is the lowest bit
- `CLK`: clock, 3.25 MHz
- `M1_N`: low during an opcode fetch or an interrupt acknowledge
- `MREQ_N`: low while the address is a memory address (also during refresh)
- `IORQ_N`: low during an input, an output or an interrupt acknowledge
- `RD_N`: low while reading
- `WR_N`: low while the data pins hold data to write
- `RFSH_N`: low while the address holds the refresh address
- `HALT_N`: low while halted, waiting for the interrupt
- `BUSAK_N`: bus acknowledge, always high
- `INT_N`, `NMI_N`, `WAIT_N`, `BUSRQ_N`, `RESET_N`: inputs, not read by the stub
- `VCC`: +5 V
- `GND`: 0 V
