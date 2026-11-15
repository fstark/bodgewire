; BW-2001 Tiny BASIC (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md; design §6): the PET 2001's
; $C000 sockets, UH1 ($C000) and UH5 ($C800). A stub for P5p's phase 3 (the editor's tests):
; COLD prints READY, then echoes each line the editor hands over as "=" and its text.

        .rom $C000, $2000
        .include "bw2001.inc"

NUM     = $30           ; 2: NUMOUT's value
NZ      = $32           ; NUMOUT: a digit printed

        .org $C000
        jmp cold
        jmp warm
        jmp numout

cold:
warm:   ldx #$FF
        txs
        cli
        ldx #0
@ready: lda ready,x
        beq @loop
        jsr EDPRINT
        inx
        bne @ready
@loop:  jsr EDLINE
        lda #'='
        jsr EDPRINT
        ldx #0
@echo:  lda LINBUF,x
        beq @end
        jsr EDPRINT
        inx
        bne @echo
@end:   lda #$0D
        jsr EDPRINT
        jmp @loop

ready:  .byte "READY", $0D, 0

; NUMOUT: A (low) and Y (high) as a signed decimal, no spaces.
numout: sta NUM
        sty NUM+1
        tya
        bpl @pos
        lda #'-'
        jsr EDPRINT
        sec
        lda #0
        sbc NUM
        sta NUM
        lda #0
        sbc NUM+1
        sta NUM+1
@pos:   lda #0
        sta NZ
        ldx #0
@power: ldy #'0'
@sub:   sec
        lda NUM
        sbc powlo,x
        pha
        lda NUM+1
        sbc powhi,x
        bcc @digit
        sta NUM+1
        pla
        sta NUM
        iny
        jmp @sub
@digit: pla
        cpy #'0'
        bne @print
        lda NZ
        beq @skip
@print: tya
        jsr EDPRINT
        lda #1
        sta NZ
@skip:  inx
        cpx #4
        bne @power
        lda NUM
        ora #'0'
        jmp EDPRINT

powlo:  .byte <10000, <1000, <100, <10
powhi:  .byte >10000, >1000, >100, >10

        .org $C7FF
        .checksum $C000, $800
        .org $CFFF
        .checksum $C800, $800
