; BW-2001 editor (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md, "The editor"; design §5): the
; PET 2001's $E000 socket (UH3). A full-screen editor on the 40 x 25 screen at $8000: the cursor
; goes anywhere, RETURN hands the logical line under it to BASIC (one row, or two joined), the
; keyboard read at each retrace (PIA1, the PET 2001's matrix), STOP the break key. It keeps the
; P5o editor contract (docs/p5o-tiny-basic/TINY_BASIC.md, "The editor contract").
;
; Entry points at $E000 (roms/bw2001.inc); the key table KEYTAB at $E700; the checksum at $E7FF.
; Zero page $40-$5F.

        .rom $E000, $800
        .include "bw2001.inc"

CROW    = $42           ; the cursor's row 0-24
CCOL    = $43           ; and column 0-39
ROWPTR  = $44           ; 2: SCREEN + 40 * CROW
UNDER   = $46           ; the screen code under the cursor while it is out
BLINK   = $47           ; retraces to the next blink
SHOWN   = $48           ; 1 while the cursor is out (blinking)
LASTKEY = $49           ; the matrix index the last scan saw, $FF none
KHEAD   = $4A           ; KEYBUF ring: written at KHEAD by the IRQ
KTAIL   = $4B           ; read at KTAIL
BRKFLAG = $4C           ; set by a STOP press
SROW    = $4D           ; where an input read began ($FF: forgotten by a scroll)
SCOL    = $4E
MODE    = $4F           ; 0 a line read, 1 an input read
PHASE   = $50           ; the blink: 1 reversed
LSTART  = $51           ; the logical line's first row
NROWS   = $52           ; its rows, 1 or 2
POS     = $53           ; the cursor's place in it, 0-79
SRC     = $54           ; 2: the logical line's first cell
CNT     = $56
KNOW    = $57           ; the scan's key, $FF none
KSHIFT  = $58           ; a SHIFT held in this scan
KROW    = $59
KBITS   = $5A
TMP     = $5B
BLINKS  = 20            ; retraces a blink phase (1/3 s)
MAXLINE = 64            ; P5o: an entered line's characters

        .org $E000
        jmp init
        jmp print
        jmp line
        jmp input
        jmp break
        jmp irq

; --- EDINIT -----------------------------------------------------------------------------------

init:   lda #0
        sta WAITING
        sta LINES
        sta SHOWN
        sta BRKFLAG
        sta KHEAD
        sta KTAIL
        sta MODE
        lda #$FF
        sta LASTKEY
        sta SROW
        jmp clear

; --- EDPRINT: A ($0D, $0C, $20-$5F); A, X, Y kept ---------------------------------------------

print:  pha
        sta TMP
        txa
        pha
        tya
        pha
        lda TMP
        cmp #$0D
        bne @ff
        lda #0
        sta CCOL
        jsr nextrow
        jmp @done
@ff:    cmp #$0C
        bne @char
        jsr clear
        jmp @done
@char:  cmp #$20
        bcc @done
        cmp #$60
        bcs @done
        jsr putch
@done:  pla
        tay
        pla
        tax
        pla
        rts

; Store A ($20-$5F) at the cursor, move right; at the row's end go on into the next row, joined
; when this row is a logical line's first, else as a new logical line.
putch:  jsr screencode
        ldy CCOL
        sta (ROWPTR),y
        inc CCOL
        lda CCOL
        cmp #COLUMNS
        bne @done
        lda #0
        sta CCOL
        ldx CROW
        lda LINKS,x
        bne @newline
        jsr nextrow
        ldx CROW
        lda #1
        sta LINKS,x
        rts
@newline:
        jsr nextrow
@done:  rts

; A ($20-$5F) as a screen code: $40-$5F are $00-$1F.
screencode:
        cmp #$40
        bcc @done
        sec
        sbc #$40
@done:  rts

; The next row, a logical line of its own (its link cleared); scrolls at the bottom.
nextrow:
        inc CROW
        lda CROW
        cmp #ROWS
        bcc @in
        jsr scroll
        lda #ROWS-1
        sta CROW
@in:    ldx CROW
        lda #0
        sta LINKS,x
        jmp rowaddr

; The next row, links kept (cursor down, RETURN); scrolls at the bottom.
downrow:
        inc CROW
        lda CROW
        cmp #ROWS
        bcc @in
        jsr scroll
        lda #ROWS-1
        sta CROW
@in:    jmp rowaddr

; ROWPTR for CROW.
rowaddr:
        ldx CROW
        lda rowlo,x
        sta ROWPTR
        lda rowhi,x
        sta ROWPTR+1
        rts

; Rows 1-24 up to 0-23, row 24 blank, the links with them. Interrupts stay on (about 8 ms). Each
; page is copied in its own loop, so no row is overwritten before it is read.
scroll: ldx #0
@p0:    lda SCREEN+$028,x
        sta SCREEN+$000,x
        inx
        bne @p0
@p1:    lda SCREEN+$128,x
        sta SCREEN+$100,x
        inx
        bne @p1
@p2:    lda SCREEN+$228,x
        sta SCREEN+$200,x
        inx
        bne @p2
@p3:    lda SCREEN+$328,x
        sta SCREEN+$300,x
        inx
        cpx #$C0
        bne @p3
        lda #$20
        ldx #COLUMNS-1
@blank: sta SCREEN+40*(ROWS-1),x
        dex
        bpl @blank
        ldx #0
@links: lda LINKS+1,x
        sta LINKS,x
        inx
        cpx #ROWS-1
        bne @links
        lda #0
        sta LINKS+ROWS-1
        sta LINKS
        lda SROW
        cmp #$FF
        beq @done
        cmp #0
        bne @up
        lda #$FF
        sta SROW
        rts
@up:    dec SROW
@done:  rts

; Blank screen, no links, the cursor top left.
clear:  lda #$20
        ldx #0
@fill:  sta SCREEN+$000,x
        sta SCREEN+$100,x
        sta SCREEN+$200,x
        sta SCREEN+$300,x
        inx
        bne @fill
        lda #0
        ldx #ROWS-1
@links: sta LINKS,x
        dex
        bpl @links
home:   lda #0
        sta CROW
        sta CCOL
        jmp rowaddr

; --- The cursor -------------------------------------------------------------------------------

; Out, reversed at once, blinking in the IRQ. Each is one short sei-cli.
showcur:
        sei
        ldy CCOL
        lda (ROWPTR),y
        and #$7F
        sta UNDER
        ora #$80
        sta (ROWPTR),y
        lda #1
        sta SHOWN
        sta PHASE
        lda #BLINKS
        sta BLINK
        cli
        rts

hidecur:
        sei
        lda SHOWN
        beq @done
        ldy CCOL
        lda UNDER
        sta (ROWPTR),y
        lda #0
        sta SHOWN
@done:  cli
        rts

; --- EDIRQ: the key scan and the blink, at each retrace -----------------------------------------

irq:    lda #$FF
        sta KNOW
        lda #0
        sta KSHIFT
        sta KROW
@row:   lda KROW
        sta PIA1PA
        lda PIA1PB      ; the first read also clears the retrace flag
        eor #$FF
        sta KBITS
        lda KROW
        asl
        asl
        asl
        tax             ; X: the matrix index, row * 8 + column
@bit:   lsr KBITS
        bcc @next
        lda KEYTAB,x
        beq @shift
        lda KNOW
        cmp #$FF
        bne @next       ; the first key found counts
        stx KNOW
        jmp @next
@shift: cpx #8*8+0
        beq @isshift
        cpx #8*8+5
        bne @next
@isshift:
        lda #1
        sta KSHIFT
@next:  inx
        txa
        and #7
        bne @bit
        inc KROW
        lda KROW
        cmp #10
        bne @row
        lda #$0F        ; no row selected between scans
        sta PIA1PA

        lda KNOW
        cmp LASTKEY
        beq @blink
        sta LASTKEY
        cmp #$FF
        beq @blink
        tax
        lda KEYTAB,x
        cmp #$03
        bne @notstop
        lda #1
        sta BRKFLAG
        jmp @blink
@notstop:
        ldy KSHIFT
        beq @queue
        cmp #$11        ; CRSR down, right, HOME, DEL: shifted up, left, CLR, INST
        beq @shifted
        cmp #$1D
        beq @shifted
        cmp #$13
        beq @shifted
        cmp #$14
        bne @queue
@shifted:
        ora #$80
@queue: ldx KHEAD
        sta KEYBUF,x
        inx
        txa
        and #7
        cmp KTAIL
        beq @blink      ; full: dropped
        sta KHEAD

@blink: lda SHOWN
        beq @done
        dec BLINK
        bne @done
        lda #BLINKS
        sta BLINK
        lda PHASE
        eor #1
        sta PHASE
        lda UNDER
        ldy PHASE
        beq @put
        ora #$80
@put:   ldy CCOL
        sta (ROWPTR),y
@done:  rts

; --- EDBREAK ----------------------------------------------------------------------------------

break:  sei
        lda BRKFLAG
        beq @no
        lda #0
        sta BRKFLAG
        cli
        sec
        rts
@no:    cli
        clc
        rts

; --- EDLINE, EDINPUT --------------------------------------------------------------------------

line:   lda #0
        sta MODE
        sta BRKFLAG
        jmp read
input:  lda #1
        sta MODE
read:   lda KHEAD       ; keys typed before the read are dropped
        sta KTAIL
        lda CROW
        sta SROW
        lda CCOL
        sta SCOL
        lda #1
        sta WAITING
        jsr showcur
@wait:  lda MODE
        bne @input
        lda #0          ; at a line read, STOP is discarded
        sta BRKFLAG
        jmp @key
@input: lda BRKFLAG
        beq @key
        lda #0
        sta BRKFLAG
        sta WAITING
        jsr hidecur
        sec
        rts
@key:   lda KTAIL
        cmp KHEAD
        beq @wait
        tax
        lda KEYBUF,x
        pha
        inx
        txa
        and #7
        sta KTAIL
        jsr hidecur
        pla
        cmp #$0D
        beq @return
        jsr dokey
        jsr showcur
        jmp @wait
@return:
        jmp enter

; LSTART, NROWS, POS and SRC for the cursor's logical line.
logical:
        ldx CROW
@up:    lda LINKS,x
        beq @first
        dex
        bne @up         ; row 0 never goes on from above
@first: stx LSTART
        lda #1
        sta NROWS
        inx
        cpx #ROWS
        bcs @pos
        lda LINKS,x
        beq @pos
        lda #2
        sta NROWS
@pos:   lda CROW
        cmp LSTART
        beq @row0
        lda #COLUMNS
        clc
        adc CCOL
        sta POS
        jmp @src
@row0:  lda CCOL
        sta POS
@src:   ldx LSTART
        lda rowlo,x
        sta SRC
        lda rowhi,x
        sta SRC+1
        rts

; A key while reading: printable, the cursor keys, HOME, CLR, DEL, INST; others do nothing.
dokey:  cmp #$20
        bcc @control
        cmp #$60
        bcs @control
        jmp typed
@control:
        cmp #$1D
        beq right
        cmp #$9D
        beq left
        cmp #$11
        bne @n1
        jmp downrow
@n1:    cmp #$91
        beq up
        cmp #$13
        bne @n2
        jmp home
@n2:    cmp #$93
        bne @n3
        jmp clear
@n3:    cmp #$14
        bne @n4
        jmp delete
@n4:    cmp #$94
        bne @n5
        jmp insert
@n5:    rts

right:  inc CCOL
        lda CCOL
        cmp #COLUMNS
        bne @done
        lda #0
        sta CCOL
        jmp downrow
@done:  rts

left:   lda CCOL
        beq @wrap
        dec CCOL
        rts
@wrap:  lda CROW
        beq @done
        dec CROW
        lda #COLUMNS-1
        sta CCOL
        jmp rowaddr
@done:  rts

up:     lda CROW
        beq @done
        dec CROW
        jmp rowaddr
@done:  rts

; A printable key: refused at place 64 of its logical line or later. A wrap from the line's
; first row goes into the second, joining it (scrolling at the bottom); the 64 limit keeps a
; second row from wrapping.
typed:  pha
        jsr logical
        pla
        ldx POS
        cpx #MAXLINE
        bcs @done
        jsr screencode
        ldy CCOL
        sta (ROWPTR),y
        inc CCOL
        lda CCOL
        cmp #COLUMNS
        bne @done
        lda #0
        sta CCOL
        lda NROWS
        cmp #2
        bne @join
        inc CROW        ; the second row is already joined
        jmp rowaddr
@join:  jsr downrow
        ldx CROW
        lda #1
        sta LINKS,x
@done:  rts

; DEL: the cell left of the cursor goes, the rest of the logical line moves left. At the line's
; start it does nothing.
delete: jsr logical
        lda POS
        beq @done
        jsr left
        dec POS
        jsr length      ; CNT = the line's cells
        ldy POS
@move:  iny
        cpy CNT
        bcs @last
        lda (SRC),y
        dey
        sta (SRC),y
        iny
        jmp @move
@last:  dey
        lda #$20
        sta (SRC),y
@done:  rts

; INST: a space at the cursor, the rest moves right, the last cell lost.
insert: jsr logical
        jsr length
        ldy CNT
        dey
@move:  cpy POS
        beq @space
        dey
        lda (SRC),y
        iny
        sta (SRC),y
        dey
        jmp @move
@space: lda #$20
        sta (SRC),y
        rts

length: lda NROWS
        cmp #2
        beq @two
        lda #COLUMNS
        sta CNT
        rts
@two:   lda #2*COLUMNS
        sta CNT
        rts

; RETURN: the logical line (from the input read's start, if the cursor is still on its line),
; cut at 64, trailing spaces trimmed, into LINBUF; the cursor to the row after it.
enter:  jsr logical
        jsr length
        ldy #0          ; the first cell taken
        lda MODE
        beq @copy
        lda SROW
        cmp LSTART
        bcc @copy
        sec
        sbc LSTART
        cmp NROWS
        bcs @copy
        tax
        lda SCOL
        cpx #0
        beq @from
        clc
        adc #COLUMNS
@from:  tay
@copy:  ldx #0
@next:  cpy CNT
        bcs @cut
        cpx #MAXLINE
        bcs @cut
        lda (SRC),y
        and #$7F
        cmp #$40
        bcc @low
        lda #$20        ; a graphic: nothing BASIC wrote
        jmp @put
@low:   cmp #$20
        bcs @put
        ora #$40
@put:   sta LINBUF,x
        iny
        inx
        jmp @next
@cut:   cpx #0
        beq @end
        lda LINBUF-1,x
        cmp #$20
        bne @end
        dex
        jmp @cut
@end:   lda #0
        sta LINBUF,x
        stx CNT
        lda LSTART
        clc
        adc NROWS
        sec
        sbc #1
        sta CROW
        lda #0
        sta CCOL
        jsr downrow
        lda #0
        sta WAITING
        inc LINES
        ldx CNT
        clc
        rts

; --- Tables -----------------------------------------------------------------------------------

rowlo:  .byte <(SCREEN+0*40), <(SCREEN+1*40), <(SCREEN+2*40), <(SCREEN+3*40), <(SCREEN+4*40)
        .byte <(SCREEN+5*40), <(SCREEN+6*40), <(SCREEN+7*40), <(SCREEN+8*40), <(SCREEN+9*40)
        .byte <(SCREEN+10*40), <(SCREEN+11*40), <(SCREEN+12*40), <(SCREEN+13*40)
        .byte <(SCREEN+14*40), <(SCREEN+15*40), <(SCREEN+16*40), <(SCREEN+17*40)
        .byte <(SCREEN+18*40), <(SCREEN+19*40), <(SCREEN+20*40), <(SCREEN+21*40)
        .byte <(SCREEN+22*40), <(SCREEN+23*40), <(SCREEN+24*40)
rowhi:  .byte >(SCREEN+0*40), >(SCREEN+1*40), >(SCREEN+2*40), >(SCREEN+3*40), >(SCREEN+4*40)
        .byte >(SCREEN+5*40), >(SCREEN+6*40), >(SCREEN+7*40), >(SCREEN+8*40), >(SCREEN+9*40)
        .byte >(SCREEN+10*40), >(SCREEN+11*40), >(SCREEN+12*40), >(SCREEN+13*40)
        .byte >(SCREEN+14*40), >(SCREEN+15*40), >(SCREEN+16*40), >(SCREEN+17*40)
        .byte >(SCREEN+18*40), >(SCREEN+19*40), >(SCREEN+20*40), >(SCREEN+21*40)
        .byte >(SCREEN+22*40), >(SCREEN+23*40), >(SCREEN+24*40)

; The PET 2001's matrix (the real 901447-05's table at $E75C, MAME pet.cpp), row 0 PB0 first:
; the character a key types; $0D RETURN, $03 STOP, $11 CRSR down, $1D CRSR right, $13 HOME,
; $14 DEL, $12 RVS; 0 for SHIFT and the unused places.
        .org $E700
KEYTAB: .byte '!', '#', '%', '&', '(', '_', $13, $1D
        .byte '"', '$', $27, $5C, ')', 0, $11, $14
        .byte 'Q', 'E', 'T', 'U', 'O', '^', '7', '9'
        .byte 'W', 'R', 'Y', 'I', 'P', 0, '8', '/'
        .byte 'A', 'D', 'G', 'J', 'L', 0, '4', '6'
        .byte 'S', 'F', 'H', 'K', ':', 0, '5', '*'
        .byte 'Z', 'C', 'B', 'M', ';', $0D, '1', '3'
        .byte 'X', 'V', 'N', ',', '?', 0, '2', '+'
        .byte 0, '@', ']', 0, '>', 0, '0', '-'
        .byte $12, '[', ' ', '<', $03, 0, '.', '='

        .org $E7FF
        .checksum $E000, $800
