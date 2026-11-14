; BW-1 self-test (docs/p5k-bw1-monitor/BW1_MONITOR.md): the 6530-003's ROM at $1800-$1BFF.
; Entered at $1800 by 1800 GO, or by the monitor's reset when the strap (U24 output 3 to PA0) is
; closed. Uses no RAM: no jsr, no stack, no zero page. Its state is in A, X, Y, S and the 003's
; port A direction register (PADD3, $1701; P5l leaves the 003's PA pins unconnected).
;
; Tests: 1 main RAM $0000-$03FF, 2 the 002's RIOT RAM $17C0, 3 the 003's $1780, 4 the 002's ROM
; ($1C00) and 5 this one sum to 0 (.checksum); the number on digit 1 while each runs. Then every
; segment of every digit (about 0.8 s), each digit alone (about 0.33 s each), and the keypad:
; PASS and the held key's code (KIM GETKEY codes), forever.
; A failure shows, forever: the test, the address's low 12 bits, the bad bits (written XOR read)
; or a ROM's sum: 123408 is $0234 bit 3, 27D501 $17D5 bit 0, 4C0001 the 002's ROM summing to 1.
; Keypad and digits as bw1_002.asm. SST must be off (this ROM is stepped); ST jumps through RAM.

        .rom $1800, $400

SAD     = $1740
PADD    = $1741
SBD     = $1742
PBDD    = $1743
PADD3   = $1701         ; the failure's test and address high nibble
DIGIT1  = $08           ; SBD of digit 1; +2 a digit
DELAY   = 100           ; dex/bne loops a digit: about 0.5 ms
SEG_P   = $F3
SEG_A   = $F7
SEG_S   = $ED

        .org $1800
SELFTEST:
        sei
        cld
        lda #$7F
        sta PADD        ; segments out
        lda #$1E
        sta PBDD        ; PB1-PB4 out: U24
        lda #DIGIT1
        sta SBD         ; digit 1 alone while a test runs
        lda TABLE3+1
        sta SAD         ; 1

; --- Test 1: main RAM, a pattern a page, then its complement ----------------------------------

RAM1:   ldx #0
@wa:    txa
        sta $0000,x     ; X
        eor #$FF
        sta $0100,x     ; X ^ $FF
        eor #$AA
        sta $0200,x     ; X ^ $55
        eor #$FF
        sta $0300,x     ; X ^ $AA
        inx
        bne @wa
@ca:    txa
        eor $0000,x
        beq @a1
        jmp FAIL10
@a1:    txa
        eor #$FF
        eor $0100,x
        beq @a2
        jmp FAIL11
@a2:    txa
        eor #$55
        eor $0200,x
        beq @a3
        jmp FAIL12
@a3:    txa
        eor #$AA
        eor $0300,x
        beq @a4
        jmp FAIL13
@a4:    inx
        bne @ca
@wb:    txa
        eor #$FF
        sta $0000,x     ; X ^ $FF
        eor #$FF
        sta $0100,x     ; X
        eor #$AA
        sta $0200,x     ; X ^ $AA
        eor #$FF
        sta $0300,x     ; X ^ $55
        inx
        bne @wb
@cb:    txa
        eor #$FF
        eor $0000,x
        beq @b1
        jmp FAIL10
@b1:    txa
        eor $0100,x
        beq @b2
        jmp FAIL11
@b2:    txa
        eor #$AA
        eor $0200,x
        beq @b3
        jmp FAIL12
@b3:    txa
        eor #$55
        eor $0300,x
        beq @b4
        jmp FAIL13
@b4:    inx
        bne @cb

; --- Tests 2 and 3: the RIOT RAMs, X ^ $A5 then X ^ $5A ---------------------------------------

RIOT2:  lda TABLE3+2
        sta SAD
        ldx #$3F
@w1:    txa
        eor #$A5
        sta $17C0,x
        dex
        bpl @w1
        ldx #$3F
@c1:    txa
        eor #$A5
        eor $17C0,x
        beq @ok1
        jmp FAIL2
@ok1:   dex
        bpl @c1
        ldx #$3F
@w2:    txa
        eor #$5A
        sta $17C0,x
        dex
        bpl @w2
        ldx #$3F
@c2:    txa
        eor #$5A
        eor $17C0,x
        beq @ok2
        jmp FAIL2
@ok2:   dex
        bpl @c2

RIOT3:  lda TABLE3+3
        sta SAD
        ldx #$3F
@w1:    txa
        eor #$A5
        sta $1780,x
        dex
        bpl @w1
        ldx #$3F
@c1:    txa
        eor #$A5
        eor $1780,x
        beq @ok1
        jmp FAIL3
@ok1:   dex
        bpl @c1
        ldx #$3F
@w2:    txa
        eor #$5A
        sta $1780,x
        dex
        bpl @w2
        ldx #$3F
@c2:    txa
        eor #$5A
        eor $1780,x
        beq @ok2
        jmp FAIL3
@ok2:   dex
        bpl @c2

; --- Tests 4 and 5: each ROM sums to 0 ---------------------------------------------------------

ROM4:   lda TABLE3+4
        sta SAD
        lda #0
        ldx #0
@p0:    clc
        adc $1C00,x
        inx
        bne @p0
@p1:    clc
        adc $1D00,x
        inx
        bne @p1
@p2:    clc
        adc $1E00,x
        inx
        bne @p2
@p3:    clc
        adc $1F00,x
        inx
        bne @p3
        tay             ; Z from the sum (inx left it set); Y the sum, shown as the bits
        beq ROM5
        jmp FAIL4

ROM5:   lda TABLE3+5
        sta SAD
        lda #0
        ldx #0
@p0:    clc
        adc $1800,x
        inx
        bne @p0
@p1:    clc
        adc $1900,x
        inx
        bne @p1
@p2:    clc
        adc $1A00,x
        inx
        bne @p2
@p3:    clc
        adc $1B00,x
        inx
        bne @p3
        tay
        beq DISPLAY
        jmp FAIL5

; --- Test 6: the display ----------------------------------------------------------------------

DISPLAY:
        lda #$7F
        sta SAD         ; every segment
        ldy #0          ; 256 frames of six digits: about 0.8 s
@frame: lda #DIGIT1
@digit: sta SBD
        ldx #DELAY
@wait:  dex
        bne @wait
        clc
        adc #2
        cmp #DIGIT1+12
        bne @digit
        dey
        bne @frame
        lda #DIGIT1     ; then each digit alone, about 0.33 s
@alone: sta SBD
        ldy #0
@outer: ldx #0
@inner: dex
        bne @inner
        dey
        bne @outer
        clc
        adc #2
        cmp #DIGIT1+12
        bne @alone

; --- The keypad: PASS and the held key's code, forever ---------------------------------------

ECHO:   ldx #0
        stx SBD         ; row 0: the last digit dark before port A floats high
        lda #0
        sta PADD
        ldy #$FE        ; the row's first code - 2
@row:   stx SBD
        lda SAD
        ora #$80
        eor #$FF
        bne @found
        iny
        iny
        iny
        iny
        iny
        iny
        iny
        inx
        inx
        cpx #6
        bne @row
        ldy #$15        ; none
        bne @show
@found: iny
        asl
        bcc @found
@show:  lda #$7F
        sta PADD
        lda #SEG_P
        ldx #0
        stx SAD
        ldx #DIGIT1
        stx SBD
        sta SAD
        ldx #DELAY
@d1:    dex
        bne @d1
        lda #SEG_A
        ldx #0
        stx SAD
        ldx #DIGIT1+2
        stx SBD
        sta SAD
        ldx #DELAY
@d2:    dex
        bne @d2
        lda #SEG_S
        ldx #0
        stx SAD
        ldx #DIGIT1+4
        stx SBD
        sta SAD
        ldx #DELAY
@d3:    dex
        bne @d3
        lda #SEG_S
        ldx #0
        stx SAD
        ldx #DIGIT1+6
        stx SBD
        sta SAD
        ldx #DELAY
@d4:    dex
        bne @d4
        lda #0          ; blank with no key
        cpy #$15
        beq @d5set
        tya
        lsr
        lsr
        lsr
        lsr
        tax
        lda TABLE3,x
@d5set: ldx #0
        stx SAD
        ldx #DIGIT1+8
        stx SBD
        sta SAD
        ldx #DELAY
@d5:    dex
        bne @d5
        lda #0
        cpy #$15
        beq @d6set
        tya
        and #$0F
        tax
        lda TABLE3,x
@d6set: ldx #0
        stx SAD
        ldx #DIGIT1+10
        stx SBD
        sta SAD
        ldx #DELAY
@d6:    dex
        bne @d6
        jmp ECHO

; --- Failures: Y the bad bits or sum, S the address's low byte, PADD3 the test and high nibble -

FAIL10: tay
        txs
        lda #$10
        sta PADD3
        jmp FAIL
FAIL11: tay
        txs
        lda #$11
        sta PADD3
        jmp FAIL
FAIL12: tay
        txs
        lda #$12
        sta PADD3
        jmp FAIL
FAIL13: tay
        txs
        lda #$13
        sta PADD3
        jmp FAIL
FAIL2:  tay
        txa
        ora #$C0        ; $17C0 + X
        tax
        txs
        lda #$27
        sta PADD3
        jmp FAIL
FAIL3:  tay
        txa
        ora #$80        ; $1780 + X
        tax
        txs
        lda #$37
        sta PADD3
        jmp FAIL
FAIL4:  ldx #0          ; Y is the sum already
        txs
        lda #$4C
        sta PADD3
        jmp FAIL
FAIL5:  ldx #0
        txs
        lda #$58
        sta PADD3

FAIL:   lda #$7F
        sta PADD
@loop:  lda PADD3
        lsr
        lsr
        lsr
        lsr
        tax
        lda TABLE3,x
        ldx #0
        stx SAD
        ldx #DIGIT1
        stx SBD
        sta SAD
        ldx #DELAY
@f1:    dex
        bne @f1
        lda PADD3
        and #$0F
        tax
        lda TABLE3,x
        ldx #0
        stx SAD
        ldx #DIGIT1+2
        stx SBD
        sta SAD
        ldx #DELAY
@f2:    dex
        bne @f2
        tsx
        txa
        lsr
        lsr
        lsr
        lsr
        tax
        lda TABLE3,x
        ldx #0
        stx SAD
        ldx #DIGIT1+4
        stx SBD
        sta SAD
        ldx #DELAY
@f3:    dex
        bne @f3
        tsx
        txa
        and #$0F
        tax
        lda TABLE3,x
        ldx #0
        stx SAD
        ldx #DIGIT1+6
        stx SBD
        sta SAD
        ldx #DELAY
@f4:    dex
        bne @f4
        tya
        lsr
        lsr
        lsr
        lsr
        tax
        lda TABLE3,x
        ldx #0
        stx SAD
        ldx #DIGIT1+8
        stx SBD
        sta SAD
        ldx #DELAY
@f5:    dex
        bne @f5
        tya
        and #$0F
        tax
        lda TABLE3,x
        ldx #0
        stx SAD
        ldx #DIGIT1+10
        stx SBD
        sta SAD
        ldx #DELAY
@f6:    dex
        bne @f6
        jmp @loop

TABLE3: .byte $BF, $86, $DB, $CF, $E6, $ED, $FD, $87     ; 0-7, as the monitor's TABLE
        .byte $FF, $EF, $F7, $FC, $B9, $DE, $F9, $F1     ; 8-F
        .checksum
