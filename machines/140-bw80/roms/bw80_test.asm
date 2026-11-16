; BW-80 test ROM (2332, IC2): docs/p5v-bw80-video/BW80_VIDEO.md, BW80_VIDEO_DESIGN.md §6.2. Run
; in place of the ZX80 ROM through --roms (a directory with IC2.rom linked to this file's .rom).
; A page of every character, normal and inverse, made the ZX80's way.
;
; $0000 a jump to the setup; $0038 the line interrupt (IM 1); $0050 the setup and the frame
; loop; $0E00-$0FFF the ZX80's character set (from machines/140-bw80/roms/zx80_ic2.bin). RAM: the display file at $4000, the stack below $4400.
;
; The display file is a HALT, then 24 rows of 32 character codes and a HALT each. A row runs at
; its echo (A15 high, $C000 up): each code (bit 6 clear) is fetched and forced to a NOP while the
; data latch keeps it for the glyph fetch in the refresh, I:R giving the character set's page;
; the HALT runs as itself. INT_N is on A6: R, loaded before each line, drops A6 in the refresh at
; the line's end; the interrupt's acknowledge starts HSYNC. This driver follows the ZX80 ROM's
; (Wearmouth's listing: MASK-INT at $0038, DISP-2 at $01AD, KEYBOARD's VSYNC at $013F-$0197)
; instruction for instruction where the timing depends on it: the R values, the instructions
; between LD R,A and JP (HL), and the interrupt routine, whose M1s clock the HSYNC flip-flops.
; Its own code is the setup and the VSYNC's delay. Code cell c (0-767, row by row): c & $3F,
; inverse ($80) when c & $40, so rows 2-3, 6-7 ... are inverse.

        .cpu z80
        .rom $0000, $1000

DFILE = $4000           ; the display file in RAM
ECHO = $8000            ; A15 high: the display file's echo, its codes forced to NOPs
STACK = $4400           ; top of the 1K RAM
ROWS = 24
COLUMNS = 32
NEWLINE = $76           ; HALT
TOP = 56                ; blank lines above the page (the ZX80's UK margin, KB-3)
BOTTOM = TOP - 1        ; blank lines below it (the ZX80 decrements its margin)
R_TOP = $EC             ; R before the top margin (the ZX80's, after its OUT)
R_LINE = $DD            ; R before every later line (DISP-2)
R_BOTTOM = $F3          ; R before the bottom margin
VSYNC_TURNS = 97        ; DJNZ turns: IN to OUT about 1270 T-states, the ZX80's 1262

        .org $0000
        DI                      ; (4)
        JP setup                ; (10)

        .org $0050
setup:  LD SP,STACK             ; (10)
        LD A,$0E                ; the character set's page, on A8-A15 in each refresh (7)
        LD I,A                  ; (9)
        IM 1                    ; (8)
        LD HL,DFILE             ; (10)
        LD (HL),NEWLINE         ; the lone HALT of the margins (10)
        INC HL                  ; (6)
        LD C,0                  ; code cell (7)
        LD D,ROWS               ; (7)
row:    LD B,COLUMNS            ; (7)
@cell:  LD A,C                  ; (4)
        AND $40                 ; (7)
        ADD A,A                 ; bit 6 to bit 7: inverse (4)
        LD E,A                  ; (4)
        LD A,C                  ; (4)
        AND $3F                 ; (7)
        OR E                    ; (4)
        LD (HL),A               ; (7)
        INC HL                  ; (6)
        INC C                   ; (4)
        DJNZ @cell              ; (13/8)
        LD (HL),NEWLINE         ; (10)
        INC HL                  ; (6)
        DEC D                   ; (4)
        JR NZ,row               ; (12/7)
frame:  IN A,($FE)              ; KBD_N: the VSYNC latch on (11)
        LD B,VSYNC_TURNS        ; (7)
        DJNZ *                  ; (13/8)
        OUT ($FF),A             ; any OUT: the VSYNC latch off (11)
        LD A,R_TOP              ; (7)
        LD B,ROWS + 1           ; the margin's HALT and the rows (7)
        LD HL,DFILE | ECHO      ; (10)
        LD C,TOP                ; (7)
        CALL display            ; the top margin and the page (17)
        LD A,R_BOTTOM           ; (7)
        INC B                   ; one row: the last HALT (4)
        DEC HL                  ; the last row's HALT (6)
        LD C,BOTTOM             ; (7)
        CALL display            ; the bottom margin (17)
        JR frame                ; (12)

display: LD R,A                 ; R wraps A6 low at the line's end (9)
        LD A,R_LINE             ; R for the lines after this one (7)
        EI                      ; (4)
        JP (HL)                 ; the row, in the echo (4)

; IM 1: the line's end. The pushed address is the byte after the HALT just run: the next row.
        .org $0038
        DEC C                   ; lines left of this row (4)
        JP NZ,again             ; (10)
        POP HL                  ; the next row (10)
        DEC B                   ; rows left (4)
        RET Z                   ; all done: back from display (11/5)
        SET 3,C                 ; 8 lines a row (8)
next:   LD R,A                  ; (9)
        EI                      ; (4)
        JP (HL)                 ; (4)
again:  POP DE                  ; the same row again: drop the pushed address (10)
        RET Z                   ; never taken: Z is clear here, a delay as the ZX80's (5)
        JR next                 ; (12)

        .org $0E00
        .incbin "zx80_ic2.bin", $0E00, $0200
