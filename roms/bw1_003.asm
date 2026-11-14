; BW-1 self-test (docs/p5k-bw1-monitor/BW1_MONITOR.md): the 6530-003's ROM at $1800-$1BFF.
; A stub for the model's first tests (P5k phase 2); phase 4 writes the self-test.

        .rom $1800, $400

        .org $1800
SELFTEST:
        jmp SELFTEST
        .checksum
