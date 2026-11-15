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
        lda #$0C        ; VIA CA2 low: the character ROM's first half (upper case), as BASIC 1
        sta VIAPCR
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

; --- The self-test (spec "The self-test"; design §4.4) -------------------------------------------
;
; Interrupts off. Page 0 is tested with no RAM at all; then $00-$01 (PTR0) are the only RAM used
; until the whole RAM has passed: no stack, no JSR. Rows are written at fixed screen addresses,
; as screen codes (AND $3F maps $20-$5F). A RAM or video RAM failure stops on its row with the
; address and the bad bits; the three values wait in screen cells and the row in S (FAIL).
; After the RAM, the ROM sums and the key echo use the stack.

PTR0    = $00           ; 2: the test's pointer
ROWP    = $02           ; 2: after the RAM: a row's address
SUM     = $04
KEYIX   = $05
CHIP    = $06
PUTY    = $07           ; putch keeps Y here

ROW2    = SCREEN+2*40   ; ZERO PAGE
ROW3    = SCREEN+3*40   ; VIDEO RAM
ROW4    = SCREEN+4*40   ; RAM
ROW11   = SCREEN+11*40  ; KEY
HEXZP   = ROW2+14-SCREEN ; where each failure's hex digits begin, as an offset in page $80
HEXVID  = ROW3+14-SCREEN
HEXRAM  = ROW4+8-SCREEN
SCRATCH = $14           ; the three values wait here, after the digits: bits, low, high

selftest:
        sei
        cld
        lda #$20        ; a blank screen
        ldx #0
@clear: sta SCREEN+$000,x
        sta SCREEN+$100,x
        sta SCREEN+$200,x
        sta SCREEN+$300,x
        inx
        bne @clear
        ldx #0
@title: lda titletext,x
        beq @zp
        and #$3F
        sta SCREEN,x
        inx
        bne @title

; Page 0, by zero page,X: no RAM used.
@zp:    ldx #0
@zw1:   txa
        eor #$A5
        sta $00,x
        inx
        bne @zw1
@zc1:   txa
        eor #$A5
        eor $00,x
        bne zpfail
        inx
        bne @zc1
@zw2:   txa
        eor #$5A
        sta $00,x
        inx
        bne @zw2
@zc2:   txa
        eor #$5A
        eor $00,x
        bne zpfail
        inx
        bne @zc2
        jmp video

zpfail: sta SCREEN+HEXZP+SCRATCH
        stx SCREEN+HEXZP+SCRATCH+1
        lda #0
        sta SCREEN+HEXZP+SCRATCH+2
        ldy #0
@label: lda zpbadtext,y
        beq @go
        and #$3F
        sta ROW2,y
        iny
        bne @label
@go:    ldx #HEXZP
        txs
        jmp fail

; The video RAM, $8000-$83FF: the whole range written, then compared, twice.
video:  ldx #0
@zpok:  lda zpoktext,x
        beq @pass1
        and #$3F
        sta ROW2,x
        inx
        bne @zpok
@pass1: lda #0
        sta PTR0
        lda #>SCREEN
        sta PTR0+1
@vw1:   ldy #0
@vw1b:  tya
        eor PTR0+1
        eor #$A5
        sta (PTR0),y
        iny
        bne @vw1b
        inc PTR0+1
        lda PTR0+1
        cmp #>SCREEN+4
        bne @vw1
        lda #>SCREEN
        sta PTR0+1
@vc1:   ldy #0
@vc1b:  tya
        eor PTR0+1
        eor #$A5
        eor (PTR0),y
        bne vfail
        iny
        bne @vc1b
        inc PTR0+1
        lda PTR0+1
        cmp #>SCREEN+4
        bne @vc1
        lda #>SCREEN
        sta PTR0+1
@vw2:   ldy #0
@vw2b:  tya
        eor PTR0+1
        eor #$5A
        sta (PTR0),y
        iny
        bne @vw2b
        inc PTR0+1
        lda PTR0+1
        cmp #>SCREEN+4
        bne @vw2
        lda #>SCREEN
        sta PTR0+1
@vc2:   ldy #0
@vc2b:  tya
        eor PTR0+1
        eor #$5A
        eor (PTR0),y
        bne vfail
        iny
        bne @vc2b
        inc PTR0+1
        lda PTR0+1
        cmp #>SCREEN+4
        bne @vc2
        jmp vpassed

; The bad bits in A, the address PTR0+1:Y. The screen is cleared first (the bits wait in $00,
; the low byte in X), then the rows written again.
vfail:  sta PTR0
        tya
        tax
        lda #$20
        ldy #0
@clear: sta SCREEN+$000,y
        sta SCREEN+$100,y
        sta SCREEN+$200,y
        sta SCREEN+$300,y
        iny
        bne @clear
        lda PTR0
        sta SCREEN+HEXVID+SCRATCH
        stx SCREEN+HEXVID+SCRATCH+1
        lda PTR0+1
        sta SCREEN+HEXVID+SCRATCH+2
        ldy #0
@title: lda titletext,y
        beq @zpok
        and #$3F
        sta SCREEN,y
        iny
        bne @title
@zpok:  ldy #0
@zp:    lda zpoktext,y
        beq @label
        and #$3F
        sta ROW2,y
        iny
        bne @zp
@label: ldy #0
@l:     lda vidbadtext,y
        beq @go
        and #$3F
        sta ROW3,y
        iny
        bne @l
@go:    ldx #HEXVID
        txs
        jmp fail

vpassed:
        lda #$20
        ldx #0
@clear: sta SCREEN+$000,x
        sta SCREEN+$100,x
        sta SCREEN+$200,x
        sta SCREEN+$300,x
        inx
        bne @clear
@title: lda titletext,x
        beq @zp
        and #$3F
        sta SCREEN,x
        inx
        bne @title
@zp:    ldx #0
@zpok:  lda zpoktext,x
        beq @vid
        and #$3F
        sta ROW2,x
        inx
        bne @zpok
@vid:   ldx #0
@vidok: lda vidoktext,x
        beq ram
        and #$3F
        sta ROW3,x
        inx
        bne @vidok

; The RAM, $0002-$1FFF: written whole (page 0 by zero page,X), then compared whole, twice.
ram:    ldx #2
@w0a:   txa
        eor #$A5
        sta $00,x
        inx
        bne @w0a
        stx PTR0
        lda #1
        sta PTR0+1
@wpa:   ldy #0
@wba:   tya
        eor PTR0+1
        eor #$A5
        sta (PTR0),y
        iny
        bne @wba
        inc PTR0+1
        lda PTR0+1
        cmp #$20
        bne @wpa
        ldx #2
@c0a:   txa
        eor #$A5
        eor $00,x
        beq @c0an
        jmp rfail0
@c0an:  inx
        bne @c0a
        lda #1
        sta PTR0+1
@cpa:   ldy #0
@cba:   tya
        eor PTR0+1
        eor #$A5
        eor (PTR0),y
        beq @cban
        jmp rfail
@cban:  iny
        bne @cba
        inc PTR0+1
        lda PTR0+1
        cmp #$20
        bne @cpa

        ldx #2
@w0b:   txa
        eor #$5A
        sta $00,x
        inx
        bne @w0b
        lda #1
        sta PTR0+1
@wpb:   ldy #0
@wbb:   tya
        eor PTR0+1
        eor #$5A
        sta (PTR0),y
        iny
        bne @wbb
        inc PTR0+1
        lda PTR0+1
        cmp #$20
        bne @wpb
        ldx #2
@c0b:   txa
        eor #$5A
        eor $00,x
        beq @c0bn
        jmp rfail0
@c0bn:  inx
        bne @c0b
        lda #1
        sta PTR0+1
@cpb:   ldy #0
@cbb:   tya
        eor PTR0+1
        eor #$5A
        eor (PTR0),y
        beq @cbbn
        jmp rfail
@cbbn:  iny
        bne @cbb
        inc PTR0+1
        lda PTR0+1
        cmp #$20
        bne @cpb
        jmp rampassed

rfail0: sta SCREEN+HEXRAM+SCRATCH
        stx SCREEN+HEXRAM+SCRATCH+1
        lda #0
        sta SCREEN+HEXRAM+SCRATCH+2
        jmp rlabel
rfail:  sta SCREEN+HEXRAM+SCRATCH
        sty SCREEN+HEXRAM+SCRATCH+1
        lda PTR0+1
        sta SCREEN+HEXRAM+SCRATCH+2
rlabel: ldy #0
@l:     lda rambadtext,y
        beq @go
        and #$3F
        sta ROW4,y
        iny
        bne @l
@go:    ldx #HEXRAM
        txs
        jmp fail

; A failure's row: S its digits' offset in page $80, the bits, low and high bytes waiting in
; the cells SCRATCH beyond. The digits written, the cells blanked, and here it stays.
fail:   tsx
        lda SCREEN+SCRATCH+2,x
        lsr
        lsr
        lsr
        lsr
        cmp #10
        bcc @h1
        adc #6
@h1:    adc #'0'
        and #$3F
        sta SCREEN,x
        lda SCREEN+SCRATCH+2,x
        and #$0F
        cmp #10
        bcc @h2
        adc #6
@h2:    adc #'0'
        and #$3F
        sta SCREEN+1,x
        lda SCREEN+SCRATCH+1,x
        lsr
        lsr
        lsr
        lsr
        cmp #10
        bcc @h3
        adc #6
@h3:    adc #'0'
        and #$3F
        sta SCREEN+2,x
        lda SCREEN+SCRATCH+1,x
        and #$0F
        cmp #10
        bcc @h4
        adc #6
@h4:    adc #'0'
        and #$3F
        sta SCREEN+3,x
        lda SCREEN+SCRATCH,x
        lsr
        lsr
        lsr
        lsr
        cmp #10
        bcc @h5
        adc #6
@h5:    adc #'0'
        and #$3F
        sta SCREEN+5,x
        lda SCREEN+SCRATCH,x
        and #$0F
        cmp #10
        bcc @h6
        adc #6
@h6:    adc #'0'
        and #$3F
        sta SCREEN+6,x
        lda #$20
        sta SCREEN+SCRATCH,x
        sta SCREEN+SCRATCH+1,x
        sta SCREEN+SCRATCH+2,x
@hang:  jmp @hang

; The RAM is good: the stack and JSR from here.
rampassed:
        ldx #$FF
        txs
        lda #<ROW4
        sta ROWP
        lda #>ROW4
        sta ROWP+1
        ldx #<ramoktext
        ldy #>ramoktext
        jsr putstr
        ldx #0
@chip:  stx CHIP
        lda romrowlo,x  ; ROWP = row 5 + CHIP
        sta ROWP
        lda romrowhi,x
        sta ROWP+1
        lda #0
        sta PTR0
        sta SUM
        lda rompages,x
        sta PTR0+1
        ldx #8          ; 2048 bytes
@page:  ldy #0
@byte:  clc
        lda SUM
        adc (PTR0),y
        sta SUM
        iny
        bne @byte
        inc PTR0+1
        dex
        bne @page
        ldx #<romtext
        ldy #>romtext
        jsr putstr
        ldx CHIP
        lda rompages,x
        jsr puthex
        lda #0
        jsr puthex
        lda SUM
        bne @bad
        ldx #<oktext
        ldy #>oktext
        jsr putstr
        jmp @next
@bad:   ldx #<badtext
        ldy #>badtext
        jsr putstr
        lda SUM
        jsr puthex
@next:  ldx CHIP
        inx
        cpx #ROMCHIPS
        bne @chip

; The keyboard: the first held key in scan order, its row, column and legend, forever.
keys:   lda #<ROW11
        sta ROWP
        lda #>ROW11
        sta ROWP+1
        lda #$FF
        sta KEYIX
        ldx #0
@row:   stx PIA1PA
        lda PIA1PB
        ldy #0
@bit:   lsr
        bcs @up
        pha
        lda KEYIX
        cmp #$FF
        bne @have
        txa
        asl
        asl
        asl
        sty SUM
        ora SUM
        sta KEYIX
@have:  pla
@up:    iny
        cpy #8
        bne @bit
        inx
        cpx #10
        bne @row
        lda #$0F
        sta PIA1PA
        ldx #<keytext
        ldy #>keytext
        jsr putstr
        lda KEYIX
        cmp #$FF
        beq @blank
        lsr
        lsr
        lsr
        ora #'0'
        jsr putch
        lda KEYIX
        and #7
        ora #'0'
        jsr putch
        lda #' '
        jsr putch
        ldx KEYIX
        lda legends,x
        bmi @name
        jsr putch
        jmp @blank
@name:  and #$7F
        tax
        ldy #0
@find:  cpx #0
        beq @print
@skip:  lda names,y
        iny
        cmp #0
        bne @skip
        dex
        jmp @find
@print: lda names,y
        beq @blank
        jsr putch
        iny
        bne @print
@blank: ldx ROWP        ; the rest of the row blank: row 11 lies in page $81 ($81B8-$81DF)
        lda #$20
@b:     cpx #<ROW11+20
        beq @again
        sta $8100,x
        inx
        bne @b
@again: jmp keys

; The text at X (low), Y (high) at ROWP, ROWP moving on.
putstr: stx PTR0
        sty PTR0+1
        ldy #0
@loop:  lda (PTR0),y
        beq @done
        jsr putch
        iny
        bne @loop
@done:  rts

; A at ROWP as a screen code, ROWP + 1; X, Y kept.
putch:  sty PUTY
        ldy #0
        and #$3F
        sta (ROWP),y
        inc ROWP
        bne @done
        inc ROWP+1
@done:  ldy PUTY
        rts

puthex: pha
        lsr
        lsr
        lsr
        lsr
        jsr @nib
        pla
        and #$0F
@nib:   cmp #10
        bcc @d
        adc #6
@d:     adc #'0'
        jmp putch

ROMCHIPS = 5
rompages:
        .byte $C0, $C8, $E0, $F0, $F8
romrowlo:
        .byte <(SCREEN+5*40), <(SCREEN+6*40), <(SCREEN+7*40), <(SCREEN+8*40), <(SCREEN+9*40)
romrowhi:
        .byte >(SCREEN+5*40), >(SCREEN+6*40), >(SCREEN+7*40), >(SCREEN+8*40), >(SCREEN+9*40)

titletext:
        .byte "BW-2001 SELF-TEST", 0
zpoktext:
        .byte "ZERO PAGE OK", 0
zpbadtext:
        .byte "ZERO PAGE BAD ", 0
vidoktext:
        .byte "VIDEO RAM OK", 0
vidbadtext:
        .byte "VIDEO RAM BAD ", 0
ramoktext:
        .byte "RAM OK", 0
rambadtext:
        .byte "RAM BAD ", 0
romtext:
        .byte "ROM ", 0
oktext: .byte " OK", 0
badtext:
        .byte " BAD ", 0
keytext:
        .byte "KEY ", 0

; Each matrix place's legend (spec "The PET it runs on"), row 0 PB0 first: a character, or
; $80 + a name's index in NAMES, or 0 unused.
legends:
        .byte '!', '#', '%', '&', '(', '_', $80+8, $80+7
        .byte '"', '$', $27, $5C, ')', 0, $80+6, $80+5
        .byte 'Q', 'E', 'T', 'U', 'O', '^', '7', '9'
        .byte 'W', 'R', 'Y', 'I', 'P', 0, '8', '/'
        .byte 'A', 'D', 'G', 'J', 'L', 0, '4', '6'
        .byte 'S', 'F', 'H', 'K', ':', 0, '5', '*'
        .byte 'Z', 'C', 'B', 'M', ';', $80+1, '1', '3'
        .byte 'X', 'V', 'N', ',', '?', 0, '2', '+'
        .byte $80+3, '@', ']', 0, '>', $80+4, '0', '-'
        .byte $80+9, '[', $80+0, '<', $80+2, 0, '.', '='
names:  .byte "SPACE", 0, "RETURN", 0, "STOP", 0, "SHIFT L", 0, "SHIFT R", 0
        .byte "DEL", 0, "CRSR DN", 0, "CRSR RT", 0, "HOME", 0, "RVS", 0

        .org $FFF9
        .checksum $F800, $800
        .word nmi, reset, irq
