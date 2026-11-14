; BW-1 bench (tests/data/bw1_bench.bwn, docs/p5j-bw1-chips/BW1_CHIPS_DESIGN.md §7): the 6530-002's
; ROM at $1C00-$1FFF (mirrored to $FC00: A13-A15 undecoded). Tests the 6102s ($0000-$03FF) and the
; 6530's RAM ($17C0-$17FF), shows the result on port A ($55: both good, $01: a 6102 bit, $02: a
; 6530 RAM bit), then counts timer interrupts on PB0-PB4 and copies PA7 (a button) to PB5.

        .rom $1C00, $400

PAD     = $1740         ; port A data
PADD    = $1741         ; port A direction
PBD     = $1742         ; port B data
PBDD    = $1743         ; port B direction
TIMER8  = $174D         ; timer write: A2 = 1, A3 = 1 (interrupt on PB7), A1 A0 = 01 (÷8)
RIOTRAM = $17C0         ; the 6530's 64 bytes
COUNT   = $00           ; interrupts counted, once the RAM tests are done
TICKS   = 100           ; 100 x 8 = 800 cycles between interrupts

        .org $1C00
reset:  sei
        cld
        ldx #$FF
        txs
        lda #$7F
        sta PADD        ; PA0-PA6 outputs, PA7 an input (the button)
        lda #$3F
        sta PBDD        ; PB0-PB5 outputs, PB7 an input (the interrupt)
        lda #0
        sta PAD
        sta PBD

; The 6102s: four patterns, written then compared. No RAM is used meanwhile.
        ldx #0
@write: txa
        sta $0000,x
        eor #$FF
        sta $0100,x
        txa
        eor #$55
        sta $0200,x
        txa
        eor #$AA
        sta $0300,x
        inx
        bne @write
@check: txa
        cmp $0000,x
        bne bad6102
        eor #$FF
        cmp $0100,x
        bne bad6102
        txa
        eor #$55
        cmp $0200,x
        bne bad6102
        txa
        eor #$AA
        cmp $0300,x
        bne bad6102
        inx
        bne @check

; The 6530's RAM.
        ldx #$3F
@riotw: txa
        eor #$A5
        sta RIOTRAM,x
        dex
        bpl @riotw
        ldx #$3F
@riotc: txa
        eor #$A5
        cmp RIOTRAM,x
        bne bad6530
        dex
        bpl @riotc

        lda #$55
        sta PAD
        lda #0
        sta COUNT
        lda #TICKS
        sta TIMER8
        cli
loop:   lda COUNT
        and #$1F
        bit PAD         ; N: PA7, the button (pulled up: 1 while not pressed)
        bpl @low
        ora #$20
@low:   sta PBD
        jmp loop

bad6102:
        lda #$01
        sta PAD
        jmp *
bad6530:
        lda #$02
        sta PAD
        jmp *

irq:    pha
        lda #TICKS
        sta TIMER8      ; again from now; clears the flag, PB7 back high
        inc COUNT
        pla
        rti
nmi:    rti

        .org $1FFA
        .word nmi, reset, irq   ; NMI, RESET, IRQ/BRK
