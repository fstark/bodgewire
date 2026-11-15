; BW-2001 kernel (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md; design §4): the PET 2001's
; $F000 sockets. UH4 ($F000-$F7FF) holds the monitor; UH7 ($F800-$FFFF) the reset, the
; interrupts, the self-test and the vectors, so the self-test runs even when UH4, UH3 or BASIC
; is dead. Memory, I/O and the entry points: roms/bw2001.inc.
;
; Reset: PIA1 set (rows out, DIAG in); DIAG (PA7) low runs the self-test before any RAM is used.
; Else the RAM is sized from $0400 ($55 then $AA a byte, up to $2000), the editor started, the
; retrace interrupt on, the banner printed, and BASIC's COLD entered.

        .rom $F000, $1000
        .include "bw2001.inc"

; --- UH4: the monitor ---------------------------------------------------------------------------

        .org $F000
        jmp monitor
        jmp monbrk

monitor:
        rts
monbrk: jmp WARM

        .org $F7FF
        .checksum $F000, $800

; --- UH7: reset, interrupts, the self-test -------------------------------------------------------

        .org $F800
        jmp selftest
        jmp reset

reset:  sei
        cld
        ldx #$FF
        txs
        lda #0          ; PIA1 port A: PA0-PA3 the keyboard row out, PA4-PA7 in (PA7 DIAG)
        sta PIA1CRA
        lda #$0F
        sta PIA1PA
        lda #$04
        sta PIA1CRA
        lda #$0F
        sta PIA1PA      ; no row selected
        lda #0          ; port B: the columns in; CB1 on its falling edge, interrupt off
        sta PIA1CRB
        sta PIA1PB
        lda #$04
        sta PIA1CRB
        lda PIA1PA
        bmi @basic
        jmp selftest    ; DIAG low: no RAM touched yet

@basic: lda #0
        ldx #$7F
@zp:    sta $00,x
        dex
        bpl @zp
        ldx #0
@pages: sta $0200,x
        sta $0300,x
        inx
        bne @pages

        lda #<PROGRAM   ; size the RAM
        sta PTR
        lda #>PROGRAM
        sta PTR+1
        ldy #0
@size:  lda #$55
        sta (PTR),y
        cmp (PTR),y
        bne @top
        lda #$AA
        sta (PTR),y
        cmp (PTR),y
        bne @top
        inc PTR
        bne @size
        inc PTR+1
        lda PTR+1
        cmp #$20
        bne @size
@top:   lda PTR
        sta MEMTOP
        lda PTR+1
        sta MEMTOP+1

        jsr EDINIT
        lda #$05        ; the retrace interrupt on
        sta PIA1CRB
        cli

        ldx #0
@title: lda banner,x
        beq @free
        jsr EDPRINT
        inx
        bne @title
@free:  sec             ; MEMTOP - PROGRAM - 2: the bytes free
        lda MEMTOP
        sbc #<(PROGRAM+2)
        pha
        lda MEMTOP+1
        sbc #>(PROGRAM+2)
        tay
        pla
        jsr NUMOUT
        ldx #0
@bytes: lda freetext,x
        beq @cold
        jsr EDPRINT
        inx
        bne @bytes
@cold:  jmp COLD

banner: .byte "*** BW-2001 TINY BASIC ***", $0D, $0D, 0
freetext:
        .byte " BYTES FREE", $0D, $0D, 0

; IRQ: a BRK goes to the monitor with this handler's three bytes on the stack; the retrace (PIA1
; CRB bit 7) runs the editor's scan.
irq:    pha
        txa
        pha
        tya
        pha
        tsx
        lda $0104,x     ; the P the 6502 pushed
        and #$10
        beq @retrace
        jmp MONBRK
@retrace:
        lda PIA1CRB
        bpl @done
        jsr EDIRQ
@done:  pla
        tay
        pla
        tax
        pla
nmi:    rti

selftest:
        jmp selftest

        .org $FFF9
        .checksum $F800, $800
        .word nmi, reset, irq
