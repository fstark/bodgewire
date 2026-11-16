; BW-80 chips bench program (P5u): tests/data/bw80_bench.bwn, docs/p5u-bw80-chips/
; BW80_CHIPS_DESIGN.md §6.2. On the ZX80's CPU, ROM, RAM and data bus resistors: tests the RAM,
; runs a line of character codes in the RAM's $C000 mirror (each fetch forced to a NOP by the
; 74LS05s while the 74LS373 latches the code), writes into the ROM's space, then reads two
; keyboard rows forever. Results in RAM: STATUS $55 when every check passed, else the code of the
; first that failed; KEYS8 and KEYS9 the last reads of the rows on A8 and A9.

        .cpu z80
        .rom $0000, $1000

STATUS = $4000          ; $55 passed; $E1 RAM, $E2 NOP line, $E3 line read, $E4 ROM read
KEYS8 = $4001           ; IN with A8 low
KEYS9 = $4002           ; IN with A9 low
LINE = $4100            ; the line of character codes
MIRROR = $8000          ; A15 high: the RAM's mirror, where an M1 fetch of a code is a NOP

        .org $0000
        DI
; RAM, pass 1: each byte gets its address's low byte xor high byte.
        LD HL,$4000
fill:   LD A,L
        XOR H
        LD (HL),A
        INC HL
        LD A,H
        CP $44
        JR NZ,fill
; Pass 2: check, and leave the complement.
        LD HL,$4000
check1: LD A,L
        XOR H
        CP (HL)
        JR NZ,ramfail
        CPL
        LD (HL),A
        INC HL
        LD A,H
        CP $44
        JR NZ,check1
; Pass 3: check the complement.
        LD HL,$4000
check2: LD A,L
        XOR H
        CPL
        CP (HL)
        JR NZ,ramfail
        INC HL
        LD A,H
        CP $44
        JR NZ,check2

; The line: codes $01-$10 (bit 6 clear) then RET ($C9, bit 6 set), run in the mirror. Executed
; as themselves, $01 would load BC and $09 add BC to HL.
        LD SP,$4400
        LD HL,LINE
        LD B,$10
        LD A,$01
copy:   LD (HL),A
        INC HL
        INC A
        DJNZ copy
        LD (HL),$C9
        LD HL,$1234
        LD BC,$5678
        CALL LINE | MIRROR
        LD A,H
        CP $12
        JR NZ,linefail
        LD A,L
        CP $34
        JR NZ,linefail
        LD A,B
        CP $56
        JR NZ,linefail
        LD A,C
        CP $78
        JR NZ,linefail
; A data read of the mirror is not an M1: not forced.
        LD A,(LINE | MIRROR)
        CP $01
        JR NZ,readfail
; A write into the ROM's space: the ROM drives the memory side, the CPU its own.
        LD A,$AA
        LD (rombyte),A
        LD A,(rombyte)
        CP $3C
        JR NZ,romfail
        LD A,$55
        LD (STATUS),A

keys:   LD BC,$FEFE
        IN A,(C)
        LD (KEYS8),A
        LD B,$FD
        IN A,(C)
        LD (KEYS9),A
        JR keys

ramfail:
        LD A,$E1
        JR fail
linefail:
        LD A,$E2
        JR fail
readfail:
        LD A,$E3
        JR fail
romfail:
        LD A,$E4
fail:   LD (STATUS),A
        JR *

        .org $0800
rombyte:
        .byte $3C
