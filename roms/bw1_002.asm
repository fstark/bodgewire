; BW-1 monitor (docs/p5k-bw1-monitor/BW1_MONITOR.md): the 6530-002's ROM at $1C00-$1FFF, our
; own code behind the KIM-1's entry points, so programs from the KIM books run.
;
; Memory (U4 decodes A10-A12, A13-A15 free): $0000-$03FF RAM; $1700 the 003's ports, $1740 the
; 002's (SAD, PADD, SBD, PBDD); $1780 and $17C0 the RIOT RAMs; $1800 the 003's ROM (the
; self-test); $1C00 this one; the 6502's vectors at $FFFA read $1FFA.
; Keypad and digits (KIM Fig. 3.5): PB1-PB4 drive U24 (74145): outputs 0-2 the keypad's rows
; (SBD $00, $02, $04), read on PA6-PA0; output 3 the strap to PA0 (SBD $06); outputs 4-9 digits
; 1-6 (SBD $08-$12), a segment lit while its PA0 (a) to PA6 (g) is high.
; Keys (GETKEY): 0-F $00-$0F, AD $10, DA $11, + $12, GO $13, PC $14, none $15.
;
; Entry points at the KIM's addresses: SAVE $1C00 (SAVE1 $1C05, SAVE2 $1C0F), NMIT $1C1C,
; IRQT $1C1F, RST $1C22, START $1C4F, AK $1EFE, SCAND $1F19, SCANDS $1F1F, KEYIN $1F40,
; CONVD $1F48, INCPT $1F63, GETKEY $1F6A, TABLE $1FE7.
; Zero page (the KIM's): saved registers $EF-$F5, INH $F9, POINTL $FA, POINTH $FB, TEMP $FC,
; TMPX $FD, MODE $FF (1: address). Stack in page 1. RIOT RAM: NMIV $17FA, IRQV $17FE.
; No adc or sbc: the monitor runs with a stopped program's D flag.

        .rom $1C00, $400

PCL     = $EF
PCH     = $F0
PREG    = $F1
SPUSER  = $F2
ACC     = $F3
YREG    = $F4
XREG    = $F5
INL     = $F8
INH     = $F9
POINTL  = $FA
POINTH  = $FB
TEMP    = $FC           ; Y in CONVD; the key in the main loop
TMPX    = $FD           ; the rows ANDed in AK
MODE    = $FF           ; 1 address, 0 data
NMIV    = $17FA
IRQV    = $17FE
SAD     = $1740
PADD    = $1741
SBD     = $1742
PBDD    = $1743
SELFTEST = $1800        ; bw1_003.asm
DIGIT1  = $08           ; SBD of digit 1; +2 a digit (U24 outputs 4-9)
STRAP   = $06           ; SBD of U24 output 3, the strap's line
DELAY   = 100           ; dey/bne loops a digit: about 0.5 ms

; --- NMI, BRK: save the registers, show the PC ---------------------------------------------

        .org $1C00
SAVE:   sta ACC
        pla
        sta PREG
SAVE1:  pla
        sta PCL
        sta POINTL
        pla
        sta PCH
        sta POINTH
SAVE2:  sty YREG
        stx XREG
        tsx
        stx SPUSER
        jsr INITS
        jmp START

        .org $1C1C
NMIT:   jmp (NMIV)
IRQT:   jmp (IRQV)

; --- Reset: the strap is read before any RAM is touched -----------------------------------

RST:    lda #0
        sta PADD        ; PA inputs (RES did it too)
        lda #$1E
        sta PBDD        ; PB1-PB4 out: U24's inputs
        lda #STRAP
        sta SBD         ; U24 output 3 low: a closed strap pulls PA0 low
        lda #$01
        bit SAD         ; at least 5 us after the write: the port's 1 us, then U24
        bne @mon
        jmp SELFTEST    ; strap closed: no RAM touched so far
@mon:   ldx #$FF
        txs
        jmp COLD

; --- The loop: show the address and its byte, read a key ----------------------------------

        .org $1C4F
START:  jsr INIT1
@up:    jsr SCAND       ; A = 0 when no key is held
        bne @up         ; every key released first
@down:  jsr SCAND
        beq @down
        jsr SCAND       ; still held a scan later
        beq @down
        jsr GETKEY
        cmp #$15
        bcs START       ; released meanwhile
        cmp #$10
        bcc HEX
        bne @da
        lda #1          ; AD
        sta MODE
        jmp START
@da:    cmp #$11
        bne @plus
        lda #0          ; DA
        sta MODE
        jmp START
@plus:  cmp #$12
        bne @go
        jsr INCPT       ; +
        jmp START
@go:    cmp #$13
        beq GOEXEC
        lda PCL         ; PC
        sta POINTL
        lda PCH
        sta POINTH
        jmp START

; A hex key: shifted into the address (AD) or into the byte there (DA).
HEX:    sta TEMP
        lda MODE
        beq @data
        ldx #4
@shift: asl POINTL
        rol POINTH
        dex
        bne @shift
        lda POINTL
        ora TEMP
        sta POINTL
        jmp START
@data:  ldy #0
        lda (POINTL),y
        asl
        asl
        asl
        asl
        ora TEMP
        sta (POINTL),y
        jmp START

; GO: the saved registers and stack pointer, then the address.
GOEXEC: ldx SPUSER
        txs
        lda POINTH
        pha
        lda POINTL
        pha
        lda PREG
        pha
        ldx XREG
        ldy YREG
        lda ACC
        rti

; After reset with the strap open: the vectors, the registers, $0200.
COLD:   stx SPUSER      ; $FF
        lda #0
        sta ACC
        sta XREG
        sta YREG
        sta PREG
        sta POINTL
        lda #<SAVE
        sta NMIV
        sta IRQV
        lda #>SAVE
        sta NMIV+1
        sta IRQV+1
        lda #$02
        sta POINTH
        jsr INITS       ; the first use of the stack
        jmp START

INITS:  lda #1
        sta MODE        ; address mode
INIT1:  lda #$1E
        sta PBDD
        lda #0
        sta PADD
        rts

; --- Display and keys ------------------------------------------------------------------------

; SCANDS: POINTH POINTL INH on the six digits, then AK.
SCANX:  lda #$7F
        sta PADD        ; segments out; SBD is on a row since the last AK: no digit lit
        ldx #DIGIT1
        ldy #3
@byte:  lda INL,y       ; y = 3 POINTH, 2 POINTL, 1 INH
        lsr
        lsr
        lsr
        lsr
        jsr CONVX
        lda INL,y
        jsr CONVX
        dey
        bne @byte
        jmp AKX

; CONVD: hex digit A (low 4 bits) on the digit X selects for about 0.5 ms, then X + 2; Y kept.
CONVX:  sty TEMP
        and #$0F
        tay
        lda TABLE,y
        ldy #0
        sty SAD         ; blank before the digit changes
        stx SBD
        sta SAD
        ldy #DELAY
@wait:  dey
        bne @wait
        inx
        inx
        ldy TEMP
        rts

; AK: A = 0 if no key is held. Row 0 is selected first, so the last digit goes dark before
; port A floats high.
AKX:    lda #0
        sta SBD
        sta PADD        ; PA inputs
        lda SAD
        ora #$80
        sta TMPX
        lda #$02
        sta SBD
        lda SAD
        ora #$80
        and TMPX
        sta TMPX
        lda #$04
        sta SBD
        lda SAD
        ora #$80
        and TMPX
        eor #$FF        ; a 1 for each column pulled low
        rts

; GETKEY: A = the key held, $15 if none; with two, the lower code.
GETKX:  lda #0
        sta PADD
        ldx #0          ; SBD of the row
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
        lda #$15
        rts
@found: iny             ; PA6 is the first 1 out of bit 7: the row's first code
        asl
        bcc @found
        tya
        rts

; --- The KIM's entry points ------------------------------------------------------------------

        .org $1EFE
AK:     jmp AKX

        .org $1F19
SCAND:  ldy #0
        lda (POINTL),y
        sta INH
SCANDS: jmp SCANX

        .org $1F40
KEYIN:  lda #0
        sta PADD
        jmp AKX

        .org $1F48
CONVD:  jmp CONVX

        .org $1F63
INCPT:  inc POINTL
        bne @done
        inc POINTH
@done:  rts

GETKEY: jmp GETKX       ; $1F6A

        .org $1FE7
TABLE:  .byte $BF, $86, $DB, $CF, $E6, $ED, $FD, $87     ; 0-7, PA0 = a ... PA6 = g
        .byte $FF, $EF, $F7, $FC, $B9, $DE, $F9, $F1     ; 8-F
        .checksum

        .org $1FFA
        .word NMIT, RST, IRQT   ; NMI, RESET, IRQ/BRK
