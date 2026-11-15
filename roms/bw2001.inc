; BW-2001 firmware, shared by its three ROMs (docs/p5p-bw2001-firmware/BW2001_FIRMWARE_DESIGN.md
; §3): the I/O, the RAM map and the entry points. No code here. Each ROM reaches another only
; through these entry points, each a JMP at a fixed address (a test reads each one's opcode).

; I/O
SCREEN   = $8000        ; 40 x 25 screen codes in the 1 KB video RAM
COLUMNS  = 40
ROWS     = 25
PIA1     = $E810        ; +0 PA or DDRA, +1 CRA, +2 PB or DDRB, +3 CRB
PIA1PA   = PIA1
PIA1CRA  = PIA1+1
PIA1PB   = PIA1+2
PIA1CRB  = PIA1+3

; Zero page: $00-$3F BASIC, $40-$5F the editor, $60-$7F the kernel, $80-$FF the player's (USR).
WAITING  = $40          ; 1 while the editor waits for keys to make a line (the harness signal)
LINES    = $41          ; lines handed over, modulo 256
MEMTOP   = $60          ; 2: the top of the program area that reset found
PTR      = $62          ; 2: the kernel's pointer

; Pages 2 and 3.
LINBUF   = $0200        ; 65: the line from the editor, $00-terminated
DIRBUF   = $0241        ; 72: the tokenised direct line, as a record
CSTACK   = $0290        ; 72: the control stack, 8 entries of 9 bytes
KEYBUF   = $02F0        ; 8: the editor's key ring
VARS     = $0300        ; 52: A-Z, low byte first
LINKS    = $0334        ; 25: 1 when the row goes on from the row above
SAVED    = $0350        ; 7: the monitor's PC (2), SR, AC, XR, YR, SP
USRRAM   = $03F0        ; 16: the player's ({RAM} is 1008)
PROGRAM  = $0400        ; the program base

; Entry points.
COLD     = $C000        ; BASIC: power-on NEW, READY, the main loop
WARM     = $C003        ; BASIC: READY on a fresh line, program and variables kept
NUMOUT   = $C006        ; BASIC: print A (low), Y (high) as a signed decimal
EDINIT   = $E000        ; editor: clear the screen, links, keys, flags
EDPRINT  = $E003        ; editor: print A (P5o "print"); A, X, Y kept
EDLINE   = $E006        ; editor: P5o "line": LINBUF, X = length
EDINPUT  = $E009        ; editor: P5o "read a line": carry set on break
EDBREAK  = $E00C        ; editor: P5o "break?": carry set once a STOP press
EDIRQ    = $E00F        ; editor: the retrace's work (keys, blink)
MONITOR  = $F000        ; monitor: USR(61440), C*
MONBRK   = $F003        ; monitor: B*, from the IRQ handler
SELFTEST = $F800        ; kernel: the self-test
RESET    = $F803        ; kernel: the reset vector's target
