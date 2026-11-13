; 110 LED computer (docs/machines/110-led-computer/LED_COMPUTER.md): a light bouncing along
; 8 LEDs. Memory map: RAM $0000-$03FF (mirrored to $1FFF), the LED latch at $8000 (any write in
; $8000-$9FFF), this ROM at $F800-$FFFF (mirrored from $E000).

        .rom $F800, $800

LEDS    = $8000         ; U7, the 74LS273
light   = $00           ; the lit LED: one bit set
dir     = $01           ; 0: moving left (towards D8), 1: moving right
STEPS   = 97            ; delay: 97 x 1286 cycles, about 125 ms at 1 MHz

        .org $F800
reset:  ldx #$FF
        txs             ; stack from $01FF down
        cld
        lda #$01
        sta light
        lda #0
        sta dir
loop:   lda light
        sta LEDS
        jsr delay
        lda dir
        bne @right
        asl light
        lda light
        cmp #$80
        bne loop
        inc dir
        jmp loop
@right: lsr light
        lda light
        cmp #$01
        bne loop
        dec dir
        jmp loop

delay:  ldy #STEPS
@outer: ldx #0
@inner: dex
        bne @inner      ; 256 x 5 - 1 cycles
        dey
        bne @outer
        rts

irq:    rti             ; IRQ_N and NMI_N are never pulled low

        .org $FFFA
        .word irq, reset, irq   ; NMI, RESET, IRQ/BRK
