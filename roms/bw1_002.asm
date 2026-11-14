; BW-1 monitor (docs/p5k-bw1-monitor/BW1_MONITOR.md): the 6530-002's ROM at $1C00-$1FFF.
; A stub for the model's first tests (P5k phase 2); phase 3 writes the monitor.

        .rom $1C00, $400

        .org $1C22
RST:    jmp RST

        .org $1FF7
        .checksum
        .org $1FFA
        .word RST, RST, RST     ; NMI, RESET, IRQ/BRK
