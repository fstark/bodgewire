; Bus load ROM for the real Z80 (2716): netlists/bus_load_z80.bwn, docs/p5s-z80/Z80_DESIGN.md
; (Bus load), in Z80 source since P5t (docs/p5t-z80-assembler/Z80_ASSEMBLER.md). A ZX80-style
; display program on the P5a circuit.
;
; $0000-$004A code; $0100-$0425 the RAM image copied to $4000 (6 HALTs, the page, 8 HALTs);
; $0600-$07FF the P5a glyphs (address $0600 + code x 8 + line).
;
; The picture: 38 rows of 8 lines, each line 207 T-states, then about 9 lines of VSYNC: 312
; lines. A row is entered by JP (HL) at its first byte in RAM's mirror at $C000 (A15 high): a
; display row's 32 characters are fetched with D6 low, so the circuit forces NOPs and shows them;
; its NEWLINE ($76) runs as HALT. A border row is a lone HALT. INT_N is on A6, and the CPU looks
; at it at the last T-state of each instruction, a fetch's T4, when the refresh address I:R is
; on the bus: LD R,A with A = $5D, EI, JP (HL), and the 35th fetch after EI shows R's 7 bits
; wrapped to $00 (A6 low). Display line: JP (HL), 32 characters, the HALT, one halted fetch =
; 35. Border line: JP (HL), the HALT, 33 halted fetches = 35. The IM 1 handler counts lines (C)
; and rows (B); both its paths take 54 T-states to JP (HL): 13 (acknowledge) + 54 + 35 x 4 =
; 207. The line across VSYNC is 1863 T-states. Each instruction's T-states are in its comment
; (taken/not taken). The bus load tests check the lines and frames on the simulator.

        .cpu z80
        .rom $0000, $0800

IMAGE = $0100           ; the RAM image in ROM
DISPLAY_RAM = $4000
IMAGE_SIZE = 6 + 24 * 33 + 8
TOP_ROWS = 6
ROWS = 24
BOTTOM_ROWS = 8
ROW_LINES = 8
R_LOAD = $80 - 35       ; R wraps to $00 at the 35th fetch after EI
DELAY = 120             ; DJNZ turns of the VSYNC delay

        .org $0000
        DI                      ; (4)
        LD SP,$4400             ; (10)
        IM 1                    ; (8)
        LD A,$0E                ; (7)
        LD I,A                  ; refresh A9, A10 high, the glyphs at $0600 (9)
        LD HL,IMAGE             ; (10)
        LD DE,DISPLAY_RAM       ; (10)
        LD BC,IMAGE_SIZE        ; (10)
        LDIR                    ; (21/16)
frame:  IN A,($FE)              ; VSYNC on (11)
        LD B,DELAY              ; (7)
        DJNZ *                  ; (13/8)
        LD E,0                  ; padding (7)
        LD E,0                  ; padding (7)
        NOP                     ; padding (4)
        OUT ($FE),A             ; VSYNC off (11)
        LD HL,DISPLAY_RAM | $8000 ; the first top-border HALT, in the mirror (10)
        LD BC,(TOP_ROWS + ROWS + BOTTOM_ROWS) * 256 + ROW_LINES ; B rows, C lines (10)
        LD A,R_LOAD             ; (7)
        CALL tail               ; the frame's lines (17)
        JR frame                ; (12)

; IM 1 handler: the pushed PC is the byte after the HALT, the next row's start.
        .org $0038
        DEC C                   ; lines left in the row (4)
        JP NZ,same              ; (10)
        POP HL                  ; the next row (10)
        DEC B                   ; (4)
        RET Z                   ; the frame done, back to frame's JR (11/5)
        SET 3,C                 ; 8 lines (8)
tail:   LD R,A                  ; (9)
        EI                      ; (4)
        JP (HL)                 ; (4)
same:   POP DE                  ; the pushed PC dropped (10)
        RET Z                   ; never taken (Z clear), 5 T-states (11/5)
        JR tail                 ; (12)

; The RAM image: the top border's HALTs, 24 rows of 32 character codes and a NEWLINE, the bottom
; border's HALTs. Codes are the 2513's 6 bits; bit 7 set shows the character inverse.
        .org IMAGE
        .byte $76, $76, $76, $76, $76, $76 ; the top border
; row 0: " BODGEWIRE P5A  BUS LOAD  SPIKE "
        .byte $20, $02, $0F, $04, $07, $05, $17, $09, $12, $05, $20
        .byte $10, $35, $01, $20, $20, $02, $15, $13, $20, $0C, $0F
        .byte $01, $04, $20, $20, $13, $10, $09, $0B, $05, $20, $76
; row 1
        .byte $20, $20, $20, $20, $20, $20, $20, $20, $20, $20, $20
        .byte $20, $20, $20, $20, $20, $20, $20, $20, $20, $20, $20
        .byte $20, $20, $20, $20, $20, $20, $20, $20, $20, $20, $76
; row 2
        .byte $80, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8A
        .byte $8B, $8C, $8D, $8E, $8F, $90, $91, $92, $93, $94, $95
        .byte $96, $97, $98, $99, $9A, $9B, $9C, $9D, $9E, $9F, $76
; row 3
        .byte $A0, $A1, $A2, $A3, $A4, $A5, $A6, $A7, $A8, $A9, $AA
        .byte $AB, $AC, $AD, $AE, $AF, $B0, $B1, $B2, $B3, $B4, $B5
        .byte $B6, $B7, $B8, $B9, $BA, $BB, $BC, $BD, $BE, $BF, $76
; row 4
        .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
        .byte $0B, $0C, $0D, $0E, $0F, $10, $11, $12, $13, $14, $15
        .byte $16, $17, $18, $19, $1A, $1B, $1C, $1D, $1E, $1F, $76
; row 5
        .byte $20, $21, $22, $23, $24, $25, $26, $27, $28, $29, $2A
        .byte $2B, $2C, $2D, $2E, $2F, $30, $31, $32, $33, $34, $35
        .byte $36, $37, $38, $39, $3A, $3B, $3C, $3D, $3E, $3F, $76
; row 6
        .byte $80, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8A
        .byte $8B, $8C, $8D, $8E, $8F, $90, $91, $92, $93, $94, $95
        .byte $96, $97, $98, $99, $9A, $9B, $9C, $9D, $9E, $9F, $76
; row 7
        .byte $A0, $A1, $A2, $A3, $A4, $A5, $A6, $A7, $A8, $A9, $AA
        .byte $AB, $AC, $AD, $AE, $AF, $B0, $B1, $B2, $B3, $B4, $B5
        .byte $B6, $B7, $B8, $B9, $BA, $BB, $BC, $BD, $BE, $BF, $76
; row 8
        .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
        .byte $0B, $0C, $0D, $0E, $0F, $10, $11, $12, $13, $14, $15
        .byte $16, $17, $18, $19, $1A, $1B, $1C, $1D, $1E, $1F, $76
; row 9
        .byte $20, $21, $22, $23, $24, $25, $26, $27, $28, $29, $2A
        .byte $2B, $2C, $2D, $2E, $2F, $30, $31, $32, $33, $34, $35
        .byte $36, $37, $38, $39, $3A, $3B, $3C, $3D, $3E, $3F, $76
; row 10
        .byte $80, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8A
        .byte $8B, $8C, $8D, $8E, $8F, $90, $91, $92, $93, $94, $95
        .byte $96, $97, $98, $99, $9A, $9B, $9C, $9D, $9E, $9F, $76
; row 11
        .byte $A0, $A1, $A2, $A3, $A4, $A5, $A6, $A7, $A8, $A9, $AA
        .byte $AB, $AC, $AD, $AE, $AF, $B0, $B1, $B2, $B3, $B4, $B5
        .byte $B6, $B7, $B8, $B9, $BA, $BB, $BC, $BD, $BE, $BF, $76
; row 12
        .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
        .byte $0B, $0C, $0D, $0E, $0F, $10, $11, $12, $13, $14, $15
        .byte $16, $17, $18, $19, $1A, $1B, $1C, $1D, $1E, $1F, $76
; row 13
        .byte $20, $21, $22, $23, $24, $25, $26, $27, $28, $29, $2A
        .byte $2B, $2C, $2D, $2E, $2F, $30, $31, $32, $33, $34, $35
        .byte $36, $37, $38, $39, $3A, $3B, $3C, $3D, $3E, $3F, $76
; row 14
        .byte $80, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8A
        .byte $8B, $8C, $8D, $8E, $8F, $90, $91, $92, $93, $94, $95
        .byte $96, $97, $98, $99, $9A, $9B, $9C, $9D, $9E, $9F, $76
; row 15
        .byte $A0, $A1, $A2, $A3, $A4, $A5, $A6, $A7, $A8, $A9, $AA
        .byte $AB, $AC, $AD, $AE, $AF, $B0, $B1, $B2, $B3, $B4, $B5
        .byte $B6, $B7, $B8, $B9, $BA, $BB, $BC, $BD, $BE, $BF, $76
; row 16
        .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
        .byte $0B, $0C, $0D, $0E, $0F, $10, $11, $12, $13, $14, $15
        .byte $16, $17, $18, $19, $1A, $1B, $1C, $1D, $1E, $1F, $76
; row 17
        .byte $20, $21, $22, $23, $24, $25, $26, $27, $28, $29, $2A
        .byte $2B, $2C, $2D, $2E, $2F, $30, $31, $32, $33, $34, $35
        .byte $36, $37, $38, $39, $3A, $3B, $3C, $3D, $3E, $3F, $76
; row 18
        .byte $80, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8A
        .byte $8B, $8C, $8D, $8E, $8F, $90, $91, $92, $93, $94, $95
        .byte $96, $97, $98, $99, $9A, $9B, $9C, $9D, $9E, $9F, $76
; row 19
        .byte $A0, $A1, $A2, $A3, $A4, $A5, $A6, $A7, $A8, $A9, $AA
        .byte $AB, $AC, $AD, $AE, $AF, $B0, $B1, $B2, $B3, $B4, $B5
        .byte $B6, $B7, $B8, $B9, $BA, $BB, $BC, $BD, $BE, $BF, $76
; row 20
        .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
        .byte $0B, $0C, $0D, $0E, $0F, $10, $11, $12, $13, $14, $15
        .byte $16, $17, $18, $19, $1A, $1B, $1C, $1D, $1E, $1F, $76
; row 21
        .byte $20, $21, $22, $23, $24, $25, $26, $27, $28, $29, $2A
        .byte $2B, $2C, $2D, $2E, $2F, $30, $31, $32, $33, $34, $35
        .byte $36, $37, $38, $39, $3A, $3B, $3C, $3D, $3E, $3F, $76
; row 22
        .byte $80, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8A
        .byte $8B, $8C, $8D, $8E, $8F, $90, $91, $92, $93, $94, $95
        .byte $96, $97, $98, $99, $9A, $9B, $9C, $9D, $9E, $9F, $76
; row 23
        .byte $A0, $A1, $A2, $A3, $A4, $A5, $A6, $A7, $A8, $A9, $AA
        .byte $AB, $AC, $AD, $AE, $AF, $B0, $B1, $B2, $B3, $B4, $B5
        .byte $B6, $B7, $B8, $B9, $BA, $BB, $BC, $BD, $BE, $BF, $76
        .byte $76, $76, $76, $76, $76, $76, $76, $76 ; the bottom border

; The P5a glyphs: 64 characters of 8 lines, dots in bits 6-2 (the 2513's 5 columns).
        .org $0600
; $00 '@'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01011100
        .byte %01010100
        .byte %01011100
        .byte %01000000
        .byte %00111000
; $01 'A'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000100
        .byte %01111100
        .byte %01000100
        .byte %01000100
        .byte %01000100
; $02 'B'
        .byte %00000000
        .byte %01111000
        .byte %01000100
        .byte %01000100
        .byte %01111000
        .byte %01000100
        .byte %01000100
        .byte %01111000
; $03 'C'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000000
        .byte %01000000
        .byte %01000000
        .byte %01000100
        .byte %00111000
; $04 'D'
        .byte %00000000
        .byte %01111000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01111000
; $05 'E'
        .byte %00000000
        .byte %01111100
        .byte %01000000
        .byte %01000000
        .byte %01111000
        .byte %01000000
        .byte %01000000
        .byte %01111100
; $06 'F'
        .byte %00000000
        .byte %01111100
        .byte %01000000
        .byte %01000000
        .byte %01111000
        .byte %01000000
        .byte %01000000
        .byte %01000000
; $07 'G'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000000
        .byte %01011100
        .byte %01000100
        .byte %01000100
        .byte %00111100
; $08 'H'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01111100
        .byte %01000100
        .byte %01000100
        .byte %01000100
; $09 'I'
        .byte %00000000
        .byte %00111000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00111000
; $0A 'J'
        .byte %00000000
        .byte %00011100
        .byte %00001000
        .byte %00001000
        .byte %00001000
        .byte %00001000
        .byte %01001000
        .byte %00110000
; $0B 'K'
        .byte %00000000
        .byte %01000100
        .byte %01001000
        .byte %01010000
        .byte %01100000
        .byte %01010000
        .byte %01001000
        .byte %01000100
; $0C 'L'
        .byte %00000000
        .byte %01000000
        .byte %01000000
        .byte %01000000
        .byte %01000000
        .byte %01000000
        .byte %01000000
        .byte %01111100
; $0D 'M'
        .byte %00000000
        .byte %01000100
        .byte %01101100
        .byte %01010100
        .byte %01010100
        .byte %01000100
        .byte %01000100
        .byte %01000100
; $0E 'N'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %01100100
        .byte %01010100
        .byte %01001100
        .byte %01000100
        .byte %01000100
; $0F 'O'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %00111000
; $10 'P'
        .byte %00000000
        .byte %01111000
        .byte %01000100
        .byte %01000100
        .byte %01111000
        .byte %01000000
        .byte %01000000
        .byte %01000000
; $11 'Q'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01010100
        .byte %01001000
        .byte %00110100
; $12 'R'
        .byte %00000000
        .byte %01111000
        .byte %01000100
        .byte %01000100
        .byte %01111000
        .byte %01010000
        .byte %01001000
        .byte %01000100
; $13 'S'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000000
        .byte %00111000
        .byte %00000100
        .byte %01000100
        .byte %00111000
; $14 'T'
        .byte %00000000
        .byte %01111100
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
; $15 'U'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %00111000
; $16 'V'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %00101000
        .byte %00010000
; $17 'W'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %01000100
        .byte %01010100
        .byte %01010100
        .byte %01010100
        .byte %00101000
; $18 'X'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %00101000
        .byte %00010000
        .byte %00101000
        .byte %01000100
        .byte %01000100
; $19 'Y'
        .byte %00000000
        .byte %01000100
        .byte %01000100
        .byte %00101000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
; $1A 'Z'
        .byte %00000000
        .byte %01111100
        .byte %00000100
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %01000000
        .byte %01111100
; $1B '['
        .byte %00000000
        .byte %00111000
        .byte %00100000
        .byte %00100000
        .byte %00100000
        .byte %00100000
        .byte %00100000
        .byte %00111000
; $1C '\\'
        .byte %00000000
        .byte %00000000
        .byte %01000000
        .byte %00100000
        .byte %00010000
        .byte %00001000
        .byte %00000100
        .byte %00000000
; $1D ']'
        .byte %00000000
        .byte %00111000
        .byte %00001000
        .byte %00001000
        .byte %00001000
        .byte %00001000
        .byte %00001000
        .byte %00111000
; $1E '^'
        .byte %00000000
        .byte %00010000
        .byte %00101000
        .byte %01000100
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
; $1F '_'
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %01111100
; $20 ' '
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
; $21 '!'
        .byte %00000000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00000000
        .byte %00010000
; $22 '"'
        .byte %00000000
        .byte %00101000
        .byte %00101000
        .byte %00101000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
; $23 '#'
        .byte %00000000
        .byte %00101000
        .byte %00101000
        .byte %01111100
        .byte %00101000
        .byte %01111100
        .byte %00101000
        .byte %00101000
; $24 '$'
        .byte %00000000
        .byte %00010000
        .byte %00111100
        .byte %01010000
        .byte %00111000
        .byte %00010100
        .byte %01111000
        .byte %00010000
; $25 '%'
        .byte %00000000
        .byte %01100000
        .byte %01100100
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %01001100
        .byte %00001100
; $26 '&'
        .byte %00000000
        .byte %00110000
        .byte %01001000
        .byte %01010000
        .byte %00100000
        .byte %01010100
        .byte %01001000
        .byte %00110100
; $27 "'"
        .byte %00000000
        .byte %00010000
        .byte %00010000
        .byte %00100000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
; $28 '('
        .byte %00000000
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %00100000
        .byte %00100000
        .byte %00010000
        .byte %00001000
; $29 ')'
        .byte %00000000
        .byte %00100000
        .byte %00010000
        .byte %00001000
        .byte %00001000
        .byte %00001000
        .byte %00010000
        .byte %00100000
; $2A '*'
        .byte %00000000
        .byte %00000000
        .byte %00010000
        .byte %01010100
        .byte %00111000
        .byte %01010100
        .byte %00010000
        .byte %00000000
; $2B '+'
        .byte %00000000
        .byte %00000000
        .byte %00010000
        .byte %00010000
        .byte %01111100
        .byte %00010000
        .byte %00010000
        .byte %00000000
; $2C ','
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00110000
        .byte %00010000
        .byte %00100000
; $2D '-'
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %01111100
        .byte %00000000
        .byte %00000000
        .byte %00000000
; $2E '.'
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %00110000
        .byte %00110000
; $2F '/'
        .byte %00000000
        .byte %00000000
        .byte %00000100
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %01000000
        .byte %00000000
; $30 '0'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01001100
        .byte %01010100
        .byte %01100100
        .byte %01000100
        .byte %00111000
; $31 '1'
        .byte %00000000
        .byte %00010000
        .byte %00110000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00010000
        .byte %00111000
; $32 '2'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %00000100
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %01111100
; $33 '3'
        .byte %00000000
        .byte %01111100
        .byte %00001000
        .byte %00010000
        .byte %00001000
        .byte %00000100
        .byte %01000100
        .byte %00111000
; $34 '4'
        .byte %00000000
        .byte %00001000
        .byte %00011000
        .byte %00101000
        .byte %01001000
        .byte %01111100
        .byte %00001000
        .byte %00001000
; $35 '5'
        .byte %00000000
        .byte %01111100
        .byte %01000000
        .byte %01111000
        .byte %00000100
        .byte %00000100
        .byte %01000100
        .byte %00111000
; $36 '6'
        .byte %00000000
        .byte %00011000
        .byte %00100000
        .byte %01000000
        .byte %01111000
        .byte %01000100
        .byte %01000100
        .byte %00111000
; $37 '7'
        .byte %00000000
        .byte %01111100
        .byte %00000100
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %00100000
        .byte %00100000
; $38 '8'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000100
        .byte %00111000
        .byte %01000100
        .byte %01000100
        .byte %00111000
; $39 '9'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %01000100
        .byte %00111100
        .byte %00000100
        .byte %00001000
        .byte %00110000
; $3A ':'
        .byte %00000000
        .byte %00000000
        .byte %00110000
        .byte %00110000
        .byte %00000000
        .byte %00110000
        .byte %00110000
        .byte %00000000
; $3B ';'
        .byte %00000000
        .byte %00000000
        .byte %00110000
        .byte %00110000
        .byte %00000000
        .byte %00110000
        .byte %00010000
        .byte %00100000
; $3C '<'
        .byte %00000000
        .byte %00001000
        .byte %00010000
        .byte %00100000
        .byte %01000000
        .byte %00100000
        .byte %00010000
        .byte %00001000
; $3D '='
        .byte %00000000
        .byte %00000000
        .byte %00000000
        .byte %01111100
        .byte %00000000
        .byte %01111100
        .byte %00000000
        .byte %00000000
; $3E '>'
        .byte %00000000
        .byte %00100000
        .byte %00010000
        .byte %00001000
        .byte %00000100
        .byte %00001000
        .byte %00010000
        .byte %00100000
; $3F '?'
        .byte %00000000
        .byte %00111000
        .byte %01000100
        .byte %00000100
        .byte %00001000
        .byte %00010000
        .byte %00000000
        .byte %00010000
