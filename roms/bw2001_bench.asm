; BW-2001 bench (tests/data/bw2001_bench.bwn, docs/p5n-bw2001-chips/BW2001_CHIPS_DESIGN.md §7): the
; ROM in UH7 at $F800-$FFFF. Tests the RAM ($0000-$1FFF) and the video RAM ($8000-$83FF) with
; X ^ page ^ $A5, shows the result on PIA2's port B ($55: good, $01: a RAM bit, $02: a video RAM
; bit), sets up the VIA (T1 free-running on PB7, its interrupt; the shift register in mode 4 on
; CB1 and CB2) and PIA1 (the retrace interrupt on CB1, the keyboard rows), then answers the
; interrupts: at each retrace it reads keyboard rows 3 (to PIA2's port A) and 8 (column 0 to
; PIA2's CA2), as the PET does, and counts on the VIA's port A.

        .rom $F800, $800

PIA1    = $E810         ; +0 PA or DDRA, +1 CRA, +2 PB or DDRB, +3 CRB
PIA2    = $E820
VIA     = $E840         ; +0 ORB +1 ORA +2 DDRB +3 DDRA +4 T1L +5 T1H +8 T2L +A SR +B ACR +D IFR +E IER
PTR     = $00           ; page pointer (page 0 is tested first)
JIFFY   = $02           ; retraces counted
HALF    = 998           ; T1: N + 2 = 1000 cycles a half period, PB7 a 2 ms square wave

        .org $F800
reset:  sei
        cld
        ldx #$FF
        txs
        lda #0          ; PIA2: port B the result, port A the row-3 keys, CA2 manual
        sta PIA2+3
        sta PIA2+1
        lda #$FF
        sta PIA2+2
        sta PIA2
        lda #$04
        sta PIA2+3
        lda #$3C
        sta PIA2+1
        lda #0
        sta PIA2+2

; Page 0, by abs,X: no RAM used meanwhile.
        ldx #0
@w0:    txa
        eor #$A5
        sta $0000,x
        inx
        bne @w0
@c0:    txa
        eor #$A5
        cmp $0000,x
        beq @ok0
        jmp badram
@ok0:   inx
        bne @c0

; Pages $01-$1F, through PTR.
        lda #0
        sta PTR
        lda #$01
        sta PTR+1
@page:  ldy #0
@w:     tya
        eor PTR+1
        eor #$A5
        sta (PTR),y
        iny
        bne @w
@c:     tya
        eor PTR+1
        eor #$A5
        cmp (PTR),y
        beq @ok
        jmp badram
@ok:    iny
        bne @c
        inc PTR+1
        lda PTR+1
        cmp #$20
        bne @page

; The video RAM, pages $80-$83.
        lda #$80
        sta PTR+1
@vpage: ldy #0
@vw:    tya
        eor PTR+1
        eor #$A5
        sta (PTR),y
        iny
        bne @vw
@vc:    tya
        eor PTR+1
        eor #$A5
        cmp (PTR),y
        beq @vok
        jmp badvideo
@vok:   iny
        bne @vc
        inc PTR+1
        lda PTR+1
        cmp #$84
        bne @vpage

; PIA1: PA0-PA3 the keyboard row, PB the columns, CB1 the retrace (falling edge, interrupt).
        lda #0
        sta PIA1+1
        sta PIA1+3
        sta PIA1+2      ; DDRB: all inputs
        lda #$0F
        sta PIA1        ; DDRA
        lda #$04
        sta PIA1+1      ; CRA: ORA
        lda #3
        sta PIA1        ; row 3
        lda #$05
        sta PIA1+3      ; CRB: ORB, CB1 falling, its interrupt on

; The VIA: PA the count, PB7 T1's; the shift register in mode 4 at T2 latch 8 (CB1 a 20 us
; clock); T1 free-running with its interrupt.
        lda #$FF
        sta VIA+3
        lda #$80
        sta VIA+2
        lda #8
        sta VIA+8
        lda #$D0
        sta VIA+$B      ; ACR: T1 free-running on PB7, SR mode 4
        lda #$0F
        sta VIA+$A
        lda #<HALF
        sta VIA+4
        lda #>HALF
        sta VIA+5
        lda #$C0
        sta VIA+$E      ; IER: T1

        lda #0
        sta JIFFY
        lda #$55
        sta PIA2+2      ; pass
        cli
loop:   jmp loop

badram: lda #$01
        sta PIA2+2
        jmp *
badvideo:
        lda #$02
        sta PIA2+2
        jmp *

irq:    pha
        bit VIA+$D      ; V: IFR bit 6, T1
        bvc @pia
        lda VIA+4       ; clears T1's flag
@pia:   lda PIA1+3      ; N: CRB bit 7, the retrace
        bpl @done
        lda #3          ; row 3 to PIA2's port A
        sta PIA1
        nop
        lda PIA1+2      ; clears the flag
        sta PIA2
        lda #8          ; row 8, column 0 to PIA2's CA2
        sta PIA1
        nop
        lda PIA1+2
        and #$01
        beq @low
        lda #$3C
        bne @set
@low:   lda #$34
@set:   sta PIA2+1
        inc JIFFY
        lda JIFFY
        sta VIA+1
@done:  pla
        rti
nmi:    rti

        .org $FFFA
        .word nmi, reset, irq   ; NMI, RESET, IRQ/BRK
