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

; --- UH4: the monitor (spec "The monitor"; design §4.3) --------------------------------------
;
; TIM's commands: .M aaaa bbbb, .: aaaa bb..., .R, .; pppp ss aa xx yy ss, .G [aaaa], .X, and .T
; for the self-test. Entered by USR(61440) (C*) or a BRK (B*, from the IRQ handler). It runs on
; the stack below the saved SP, so .G goes back to the caller's frames; only .X and .T reset S.

MADDR   = $64           ; 2: an address parsed or dumped
MEND    = $66           ; 2: .M's end
MBYTE   = $68           ; HEX2's value
MX      = $69           ; the place in LINBUF
MREGS   = $6A           ; 7: a .; line's fields before they are kept
MTMP    = $71           ; scratch

        .org $F000
        jmp monitor
        jmp monbrk

; USR(61440): JSR'd from BASIC. The saved PC is the return address + 1, the SP after it.
monitor:
        php
        sta SAVED+3
        stx SAVED+4
        sty SAVED+5
        pla
        sta SAVED+2
        pla
        clc
        adc #1
        sta SAVED
        pla
        adc #0
        sta SAVED+1
        tsx
        stx SAVED+6
        lda #'C'
        jmp mentry

; A BRK: the IRQ handler's Y, X, A, then the 6502's P and PC (the BRK + 2).
monbrk: pla
        sta SAVED+5
        pla
        sta SAVED+4
        pla
        sta SAVED+3
        pla
        sta SAVED+2
        pla
        sta SAVED
        pla
        sta SAVED+1
        tsx
        stx SAVED+6
        cld
        lda #'B'
mentry: cli             ; the editor's keys come from the retrace interrupt
        pha
        lda CCOL
        beq @fresh
        lda #$0D
        jsr EDPRINT
@fresh: pla
        jsr EDPRINT
        lda #'*'
        jsr EDPRINT
        lda #$0D
        jsr EDPRINT
        jsr regs
mloop:  lda #'.'
        jsr EDPRINT
        jsr EDLINE
        ldx #0
        stx MX
        jsr mspace
        lda LINBUF,x
        cmp #'.'
        bne @cmd
        inx
@cmd:   lda LINBUF,x
        beq mloop       ; an empty command
        inx
        stx MX
        cmp #'M'
        beq mdump
        cmp #':'
        bne @n1
        jmp mstore
@n1:    cmp #'R'
        bne @n2
        jsr regs
        jmp mloop
@n2:    cmp #';'
        bne @n3
        jmp msetregs
@n3:    cmp #'G'
        bne @n4
        jmp mgo
@n4:    cmp #'X'
        bne @n5
        jmp WARM
@n5:    cmp #'T'
        bne mbad
        jmp SELFTEST
mbad:   lda #'?'
        jsr EDPRINT
        lda #$0D
        jsr EDPRINT
        jmp mloop

; .M aaaa bbbb: rows of 8 from aaaa & $FFF8 while the row's start is not past bbbb; STOP ends.
mdump:  jsr hex4
        bcs mbad
        lda MADDR
        and #$F8
        sta PTR
        lda MADDR+1
        sta PTR+1
        jsr hex4
        bcs @bad
        lda MADDR
        sta MEND
        lda MADDR+1
        sta MEND+1
@row:   lda MEND        ; PTR > MEND: done
        cmp PTR
        lda MEND+1
        sbc PTR+1
        bcc @done
        jsr EDBREAK
        bcs @done
        lda #'.'
        jsr EDPRINT
        lda #':'
        jsr EDPRINT
        lda #' '
        jsr EDPRINT
        lda PTR+1
        jsr hexout
        lda PTR
        jsr hexout
        ldy #0
@byte:  lda #' '
        jsr EDPRINT
        lda (PTR),y
        jsr hexout
        iny
        cpy #8
        bne @byte
        lda #$0D
        jsr EDPRINT
        clc
        lda PTR
        adc #8
        sta PTR
        bcc @row
        inc PTR+1
        bne @row        ; past $FFFF: done
@done:  jmp mloop
@bad:   jmp mbad

; .: aaaa bb ...: up to 8 bytes, each written as it is read; it stops at the first that is not
; one (the rest of an edited dump row).
mstore: jsr hex4
        bcs @bad
        jsr hex2
        bcs @bad
        ldy #0
@byte:  lda MBYTE
        sta (MADDR),y
        iny
        cpy #8
        beq @done
        sty MEND
        jsr hex2
        ldy MEND
        bcc @byte
@done:  jmp mloop
@bad:   jmp mbad

; .; pppp ss aa xx yy ss: all six fields, then kept.
msetregs:
        jsr hex4
        bcs @bad
        lda MADDR
        sta MREGS
        lda MADDR+1
        sta MREGS+1
        ldy #2
@field: sty MEND
        jsr hex2
        bcs @bad
        ldy MEND
        lda MBYTE
        sta MREGS,y
        iny
        cpy #7
        bne @field
        ldy #6
@keep:  lda MREGS,y
        sta SAVED,y
        dey
        bpl @keep
        jmp mloop
@bad:   jmp mbad

; .G [aaaa]: the saved registers and SP, then RTI to the PC.
mgo:    jsr mspace
        lda LINBUF,x
        beq @go
        jsr hex4
        bcs @bad
        lda MADDR
        sta SAVED
        lda MADDR+1
        sta SAVED+1
@go:    sei
        ldx SAVED+6
        txs
        lda SAVED+1
        pha
        lda SAVED
        pha
        lda SAVED+2
        pha
        ldy SAVED+5
        ldx SAVED+4
        lda SAVED+3
        rti
@bad:   jmp mbad

; The header and the .; row.
regs:   ldx #0
@head:  lda regtext,x
        beq @row
        jsr EDPRINT
        inx
        bne @head
@row:   lda #'.'
        jsr EDPRINT
        lda #';'
        jsr EDPRINT
        lda #' '
        jsr EDPRINT
        lda SAVED+1
        jsr hexout
        lda SAVED
        jsr hexout
        ldx #2
@reg:   lda #' '
        jsr EDPRINT
        lda SAVED,x
        jsr hexout
        inx
        cpx #7
        bne @reg
        lda #$0D
        jmp EDPRINT

regtext:
        .byte "   PC  SR AC XR YR SP", $0D, 0

; Spaces in LINBUF from MX skipped; X = MX.
mspace: ldx MX
@loop:  lda LINBUF,x
        cmp #' '
        bne @done
        inx
        bne @loop
@done:  stx MX
        rts

; Exactly 4 hex digits after spaces into MADDR (high first): carry set if not.
hex4:   jsr hex2
        bcs @bad
        lda MBYTE
        sta MADDR+1
        jsr digits      ; the low byte follows with no space
        bcs @bad
        sta MADDR
        clc
@bad:   rts

; Exactly 2 hex digits after spaces into MBYTE: carry set if not.
hex2:   jsr mspace
        jsr digits
        bcs @bad
        sta MBYTE
@bad:   rts

; Two hex digits at MX into A, MX past them; carry set if not.
digits: ldx MX
        lda LINBUF,x
        jsr nibble
        bcs @bad
        asl
        asl
        asl
        asl
        sta MTMP
        inx
        lda LINBUF,x
        jsr nibble
        bcs @bad
        ora MTMP
        inx
        stx MX
        clc
@bad:   rts

nibble: cmp #'0'
        bcc @bad
        cmp #'9'+1
        bcc @digit
        cmp #'A'
        bcc @bad
        cmp #'F'+1
        bcs @bad
        sbc #'A'-10-1   ; carry clear here
        clc
        rts
@digit: and #$0F
        clc
        rts
@bad:   sec
        rts

; A as two hex digits.
hexout: pha
        lsr
        lsr
        lsr
        lsr
        jsr @nib
        pla
        and #$0F
@nib:   cmp #10
        bcc @d
        adc #'A'-10-1   ; carry set here
        jmp EDPRINT
@d:     ora #'0'
        jmp EDPRINT

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
