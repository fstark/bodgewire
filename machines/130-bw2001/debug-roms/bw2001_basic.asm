; BW-2001 Tiny BASIC (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md; design §6): the PET 2001's
; $C000 sockets, UH1 ($C000) and UH5 ($C800). The dialect is P5o's, as its spec says
; (docs/p5o-tiny-basic/TINY_BASIC.md) and as its reference interpreter runs it
; (tests/basic/tiny_basic.py): the same checks in the same order, so the same error comes first.
;
; The program from $0400 in P5o's layout, its top MEMTOP (the kernel's RAM sizing); the
; variables at VARS; the direct line tokenised into DIRBUF; FOR and GOSUB in CSTACK. The editor
; is reached through its entry points only (machines/130-bw2001/debug-roms/bw2001.inc). Zero page $00-$3F.

        .rom $C000, $2000
        .include "bw2001.inc"

TXT     = $00           ; 2: the byte being read
CURLINE = $02           ; 2: the line's record, 0 for the direct line
TOP     = $04           ; 2: the program's end: its 00 00
ACC     = $06           ; 2: the expression's value
ARG     = $08           ; 2: a left operand
T0      = $0A           ; 2
T1      = $0C           ; 2
DEPTH   = $0E           ; P5o's expression depth
CSP     = $0F           ; CSTACK entries
COL     = $10           ; BASIC's column
SEED    = $11           ; 2: RND's R
ERRSP   = $13           ; S at the main loop
LNUM    = $14           ; 2: the line number entered
SRCP    = $16           ; 2: block moves
DSTP    = $18           ; 2
COUNT   = $1A           ; 2
NEWLEN  = $1C           ; the new record's length, 0 to delete
OLDLEN  = $1D           ; the old one's, 0 if none
W       = $1E           ; tokenise: the output index
QUOTE   = $1F           ; tokenise: inside quotes
SIGN    = $20
VAR     = $21           ; a variable's index, $FF none
FIRST   = $22           ; 2: LIST's range
LAST    = $24           ; 2
NL      = $26           ; PRINT: a new line at the end
PROMPT  = $27           ; 2: INPUT's prompt text
PLEN    = $29           ; its length, 0 none
POSP    = $2A           ; 2: a record
KP      = $2C           ; 2: the keyword table
KW      = $2E           ; the keyword's token
KX      = $2F           ; tokenise: where the match began
NUM     = $30           ; 2: NUMOUT
NZ      = $32
REM_    = $33           ; 2: division's remainder
FSTART  = $35           ; 2: FOR's start, limit, step
FLIM    = $37           ; 2
FSTEP   = $39           ; 2
KC      = $3B
ECODE   = $3C
BAD     = $3D           ; a line number past 9999

TPRINT  = $80           ; P5o's tokens
TINPUT  = $81
TLET    = $82
TIF     = $83
TGOTO   = $84
TGOSUB  = $85
TRETURN = $86
TFOR    = $87
TNEXT   = $88
TEND    = $89
TREM    = $8A
TPOKE   = $8B
TCLS    = $8C
TLIST   = $8D
TRUN    = $8E
TNEW    = $8F
TTHEN   = $90
TTO     = $91
TSTEP   = $92
TAND    = $93
TOR     = $94
TNOT    = $95
TPEEK   = $96
TRND    = $97
TABS    = $98
TUSR    = $99

ESYNTAX = 1             ; P5o's error codes
ENOLINE = 2
ENOGOSUB = 3
ENOFOR  = 4
EDEEP   = 5
EDIVIDE = 6
EVALUE  = 7
EMEMORY = 8
EDIRECT = 9

ENTRY   = 9             ; CSTACK: type ($80 GOSUB, else FOR's variable), limit, step, line, text
GOSUBT  = $80

        .org $C000
        jmp cold
        jmp warm
        jmp numout

; --- The main loop (design §6.2) ----------------------------------------------------------------

cold:   jsr new
        lda #0
        sta COL
        sta CURLINE
        sta CURLINE+1
        jmp ready

warm:   ldx #$FF
        txs
        cli
        lda #0
        sta COL
        sta CURLINE
        sta CURLINE+1
ready:  ldx #$FF
        txs
        jsr fresh
        ldx #0
@text:  lda readytext,x
        beq @nl
        jsr outc
        inx
        bne @text
@nl:    jsr newline
loop:   ldx #$FF
        txs
        stx ERRSP
        jsr EDLINE
        lda #0
        sta COL
        sta CURLINE
        sta CURLINE+1
        ldx #0
@space: lda LINBUF,x
        cmp #' '
        bne @first
        inx
        bne @space
@first: cmp #'0'
        bcc direct
        cmp #'9'+1
        bcs direct
        jmp progline

direct: jsr tokenise
        lda #0
        sta CSP
        lda #<(DIRBUF+3)
        sta TXT
        lda #>(DIRBUF+3)
        sta TXT+1
        jmp nextstmt

finished:
        jmp ready

readytext:
        .byte "READY", 0

; --- Running (design §6.5) ---------------------------------------------------------------------

; Before each statement the break key; after it ':', the line's end, or a syntax error.
nextstmt:
        jsr EDBREAK
        bcc @run
        jmp dobreak
@run:   jsr statement
endstmt:
        jsr peek
        cmp #':'
        bne @eol
        jsr adv
        jmp nextstmt
@eol:   cmp #0
        beq eol
        jmp syntax
eol:    lda CURLINE
        ora CURLINE+1
        beq finished
        ldy #2
        lda (CURLINE),y
        clc
        adc CURLINE
        sta CURLINE
        bcc @start
        inc CURLINE+1
@start: jsr startline
        jmp nextstmt

; TXT at CURLINE's text; at the end marker the program is over.
startline:
        ldy #0
        lda (CURLINE),y
        iny
        ora (CURLINE),y
        beq @end
        clc
        lda CURLINE
        adc #3
        sta TXT
        lda CURLINE+1
        adc #0
        sta TXT+1
        rts
@end:   lda #0
        sta CURLINE
        sta CURLINE+1
        jmp finished

; A statement: empty at ':' or the end, LET by its letter, else by its token.
statement:
        jsr peek
        cmp #0
        beq @empty
        cmp #':'
        beq @empty
        cmp #'A'
        bcc @token
        cmp #'Z'+1
        bcs @token
        jmp let
@token: cmp #TPRINT
        bcc @bad
        cmp #TNEW+1
        bcs @bad
        jsr adv
        sec
        sbc #TPRINT
        tax
        lda stmtlo,x
        sta T0
        lda stmthi,x
        sta T0+1
        jmp (T0)
@bad:   jmp syntax
@empty: rts

stmtlo: .byte <print, <input, <let, <if, <goto, <gosub, <return, <for
        .byte <next, <end, <rem, <poke, <cls, <list, <run, <new_
stmthi: .byte >print, >input, >let, >if, >goto, >gosub, >return, >for
        .byte >next, >end, >rem, >poke, >cls, >list, >run, >new_

; --- Statements ---------------------------------------------------------------------------------

let:    jsr variable
        sta VAR
        lda #'='
        jsr expect
        jsr expr
        ldx VAR
        jmp setvar

print:  lda #1
        sta NL
@loop:  jsr peek
        cmp #0
        beq @end
        cmp #':'
        beq @end
        cmp #';'
        bne @comma
        jsr adv
        lda #0
        sta NL
        jmp @loop
@comma: cmp #','
        bne @string
        jsr adv
@tab:   lda #' '
        jsr outc
        lda COL
        and #7
        bne @tab
        lda #0
        sta NL
        jmp @loop
@string:
        cmp #'"'
        bne @expr
        jsr adv
@char:  ldy #0
        lda (TXT),y
        beq @item
        cmp #'"'
        beq @close
        jsr outc
        jsr adv
        jmp @char
@close: jsr adv
        jmp @item
@expr:  jsr expr
        jsr printacc
@item:  lda #1
        sta NL
        jsr peek
        cmp #0
        beq @end
        cmp #':'
        beq @end
        cmp #';'
        beq @loop
        cmp #','
        beq @loop
        jmp syntax
@end:   lda NL
        beq @done
        jsr newline
@done:  rts

input:  jsr indirect
        lda #0
        sta PLEN
        jsr peek
        cmp #'"'
        bne @var
        jsr adv
        lda TXT
        sta PROMPT
        lda TXT+1
        sta PROMPT+1
@count: ldy #0
        lda (TXT),y
        beq @noclose
        cmp #'"'
        beq @closed
        inc PLEN
        jsr adv
        jmp @count
@noclose:
        jmp syntax
@closed:
        jsr adv
        lda #';'
        jsr expect
@var:   jsr variable
        sta VAR
@ask:   ldy #0
@prompt:
        cpy PLEN
        beq @mark
        lda (PROMPT),y
        jsr outc
        iny
        jmp @prompt
@mark:  lda #'?'
        jsr outc
        lda #' '
        jsr outc
        jsr EDINPUT
        bcc @got
        jmp dobreak
@got:   lda #0
        sta COL
        jsr answer
        bcs @redo
        ldx VAR
        jsr setvar
        lda #0
        sta PLEN
        jsr peek
        cmp #','
        bne @done
        jsr adv
        jmp @var
@done:  rts
@redo:  ldx #0
@r:     lda redotext,x
        beq @rnl
        jsr outc
        inx
        bne @r
@rnl:   jsr newline
        jmp @ask

redotext:
        .byte "REDO", 0

; INPUT's answer in LINBUF: spaces, an optional sign, 1-5 digits worth 0-65535, spaces. ACC, or
; carry set for REDO.
answer: ldx #0
        stx SIGN
        stx ACC
        stx ACC+1
@lead:  lda LINBUF,x
        cmp #' '
        bne @sign
        inx
        bne @lead
@sign:  cmp #'+'
        beq @signed
        cmp #'-'
        bne @digits
        lda #1
        sta SIGN
@signed:
        inx
@digits:
        ldy #0
@digit: lda LINBUF,x
        cmp #'0'
        bcc @trail
        cmp #'9'+1
        bcs @trail
        cpy #5
        beq @bad
        and #$0F
        jsr times10
        bcs @bad
        inx
        iny
        jmp @digit
@trail: cpy #0
        beq @bad
@sp:    lda LINBUF,x
        cmp #' '
        bne @end
        inx
        bne @sp
@end:   cmp #0
        bne @bad
        lda SIGN
        beq @ok
        jsr negate
@ok:    clc
        rts
@bad:   sec
        rts

if:     jsr expr
        lda #TTHEN
        jsr expect
        lda ACC
        ora ACC+1
        bne @true
        pla             ; false: the rest of the line is skipped
        pla
        jmp eol
@true:  jsr peek
        cmp #'0'
        bcc @statement
        cmp #'9'+1
        bcs @statement
        jsr number
        jmp gotoacc
@statement:
        jmp statement

goto:   jsr expr
gotoacc:
        jsr findline
        lda CURLINE
        ora CURLINE+1
        bne @program
        lda #0
        sta CSP
@program:
        lda POSP
        sta CURLINE
        lda POSP+1
        sta CURLINE+1
        jsr startline
        pla             ; jumped: no end-of-statement check
        pla
        jmp nextstmt

gosub:  jsr indirect
        jsr expr
        lda #GOSUBT
        jsr push
        jmp gotoacc

return: ldx CSP
@pop:   beq @none
        dex
        jsr entryof
        lda CSTACK,y
        cmp #GOSUBT
        bne @pop
        stx CSP
        jmp resume
@none:  stx CSP
        lda #ENOGOSUB
        jmp error

for:    jsr variable
        sta VAR
        lda #'='
        jsr expect
        jsr expr
        lda ACC
        sta FSTART
        lda ACC+1
        sta FSTART+1
        lda #TTO
        jsr expect
        jsr expr
        lda ACC
        sta FLIM
        lda ACC+1
        sta FLIM+1
        lda #1
        sta FSTEP
        lda #0
        sta FSTEP+1
        jsr peek
        cmp #TSTEP
        bne @set
        jsr adv
        jsr expr
        lda ACC
        sta FSTEP
        lda ACC+1
        sta FSTEP+1
@set:   lda FSTART
        sta ACC
        lda FSTART+1
        sta ACC+1
        ldx VAR
        jsr setvar
        lda VAR
        jsr findfor
        bcc @push
        stx CSP         ; that FOR and everything above it dropped
@push:  lda FLIM
        sta ACC
        lda FLIM+1
        sta ACC+1
        lda FSTEP
        sta ARG
        lda FSTEP+1
        sta ARG+1
        lda VAR
        jmp push

next:   lda #$FF
        sta VAR
        jsr peek
        cmp #0
        beq @find
        cmp #':'
        beq @find
        jsr variable
        sta VAR
@find:  lda VAR
        jsr findfor
        bcs @found
        lda #ENOFOR
        jmp error
@found: inx             ; the entries above it dropped
        stx CSP
        dex
        jsr entryof     ; Y: the entry
        sty KC
        lda CSTACK,y
        asl
        tax             ; the variable
        clc
        lda VARS,x
        adc CSTACK+3,y
        sta VARS,x
        sta ACC
        lda VARS+1,x
        adc CSTACK+4,y
        sta VARS+1,x
        sta ACC+1
        bvs nextpop     ; past 16 bits signed
        lda CSTACK+1,y
        sta ARG
        lda CSTACK+2,y
        sta ARG+1
        jsr scmp        ; the limit (ARG) against the variable (ACC)
        ldy KC
        ldx CSTACK+4,y
        bmi @down
        cmp #$FF        ; step >= 0: past when the limit is below the variable
        beq nextpop
        jmp resume1
@down:  cmp #1          ; step < 0: past when the limit is above it
        beq nextpop
resume1:
        ldx CSP
        dex
resume: jsr entryof     ; CURLINE and TXT from entry X
        lda CSTACK+5,y
        sta CURLINE
        lda CSTACK+6,y
        sta CURLINE+1
        lda CSTACK+7,y
        sta TXT
        lda CSTACK+8,y
        sta TXT+1
        rts
nextpop:
        dec CSP
        rts

end:    pla
        pla
        jmp finished

rem:    ldy #0
@skip:  lda (TXT),y
        beq @done
        jsr adv
        jmp @skip
@done:  rts

poke:   jsr expr
        lda ACC+1
        pha
        lda ACC
        pha
        lda #','
        jsr expect
        jsr expr
        pla
        sta T0
        pla
        sta T0+1
        ldy #0
        lda ACC
        sta (T0),y
        rts

cls:    lda #$0C
        jmp outc

list:   lda #1
        sta FIRST
        lda #0
        sta FIRST+1
        lda #<9999
        sta LAST
        lda #>9999
        sta LAST+1
        jsr peek
        cmp #0
        beq @go
        cmp #':'
        beq @go
        jsr number
        lda ACC
        sta FIRST
        sta LAST
        lda ACC+1
        sta FIRST+1
        sta LAST+1
        jsr peek
        cmp #'-'
        bne @check
        jsr adv
        jsr number
        lda ACC
        sta LAST
        lda ACC+1
        sta LAST+1
@check: lda FIRST
        ldx FIRST+1
        jsr inrange
        bcs @bad
        lda LAST
        ldx LAST+1
        jsr inrange
        bcc @go
@bad:   lda #EVALUE
        jmp error
@go:    lda #<PROGRAM
        sta POSP
        lda #>PROGRAM
        sta POSP+1
@line:  ldy #0
        lda (POSP),y
        sta ACC
        iny
        lda (POSP),y
        sta ACC+1
        ora ACC
        beq @done
        lda ACC         ; FIRST <= n <= LAST, unsigned
        cmp FIRST
        lda ACC+1
        sbc FIRST+1
        bcc @skip
        lda LAST
        cmp ACC
        lda LAST+1
        sbc ACC+1
        bcc @skip
        jsr EDBREAK
        bcc @show
        jmp dobreak
@show:  jsr printacc
        lda #' '
        jsr outc
        ldy #3
@text:  lda (POSP),y
        beq @eol
        sty KC
        jsr outtoken
        ldy KC
        iny
        bne @text
@eol:   jsr newline
@skip:  ldy #2
        lda (POSP),y
        clc
        adc POSP
        sta POSP
        bcc @line
        inc POSP+1
        bne @line
@done:  rts

run:    jsr clear
        pla
        pla
        lda #<PROGRAM
        sta CURLINE
        lda #>PROGRAM
        sta CURLINE+1
        jsr startline
        jmp nextstmt

new_:   jsr new
        pla
        pla
        jmp finished

; --- The control stack ----------------------------------------------------------------------------

; Y = entry X's offset in CSTACK.
entryof:
        lda offsets,x
        tay
        rts
offsets:
        .byte 0, ENTRY, 2*ENTRY, 3*ENTRY, 4*ENTRY, 5*ENTRY, 6*ENTRY, 7*ENTRY

; Pushes an entry of type A: ACC the limit, ARG the step (FOR), CURLINE and TXT the place to
; go back to. A ninth is TOO DEEP.
push:   ldx CSP
        cpx #8
        bcc @room
        lda #EDEEP
        jmp error
@room:  pha
        jsr entryof
        pla
        sta CSTACK,y
        lda ACC
        sta CSTACK+1,y
        lda ACC+1
        sta CSTACK+2,y
        lda ARG
        sta CSTACK+3,y
        lda ARG+1
        sta CSTACK+4,y
        lda CURLINE
        sta CSTACK+5,y
        lda CURLINE+1
        sta CSTACK+6,y
        lda TXT
        sta CSTACK+7,y
        lda TXT+1
        sta CSTACK+8,y
        inc CSP
        rts

; The innermost FOR of variable A ($FF: any) above the innermost GOSUB: carry set, X its index.
findfor:
        sta KC
        ldx CSP
@down:  beq @none
        dex
        jsr entryof
        lda CSTACK,y
        cmp #GOSUBT
        beq @none
        lda KC
        cmp #$FF
        beq @found
        cmp CSTACK,y
        bne @down2
@found: sec
        rts
@down2: cpx #0
        jmp @down
@none:  clc
        rts

; ILLEGAL DIRECT in the direct line.
indirect:
        lda CURLINE
        ora CURLINE+1
        bne @ok
        lda #EDIRECT
        jmp error
@ok:    rts

; --- Lines (design §6.4) ----------------------------------------------------------------------------

; A program line in LINBUF from X: its number (1-9999, else BAD VALUE), the spaces after it,
; then its text stored, or the line deleted when there is none.
progline:
        lda #0
        sta LNUM
        sta LNUM+1
        sta BAD
@digit: lda LINBUF,x
        cmp #'0'
        bcc @end
        cmp #'9'+1
        bcs @end
        and #$0F
        pha
        lda LNUM
        sta ACC
        lda LNUM+1
        sta ACC+1
        pla
        jsr times10
        bcc @keep
        lda #1
        sta BAD
@keep:  lda ACC
        sta LNUM
        lda ACC+1
        sta LNUM+1
        inx
        bne @digit
@end:   lda BAD
        bne @bad
        lda LNUM
        ldx LNUM+1
        jsr inrange
        bcs @bad
        ldx #0          ; back past the digits: they were counted from the line's start
@skip:  lda LINBUF,x
        cmp #' '
        beq @next
        cmp #'0'
        bcc @text
        cmp #'9'+1
        bcs @text
@next:  inx
        bne @skip
@text:  lda #0
        sta NEWLEN
        lda LINBUF,x
        beq store
        jsr tokenise
        lda LNUM
        sta DIRBUF
        lda LNUM+1
        sta DIRBUF+1
        lda NEWLEN
        sta DIRBUF+2
        jmp store
@bad:   lda #EVALUE
        jmp error

; NEWLEN bytes of DIRBUF in place of line LNUM (OLDLEN bytes, 0 if none), if they fit below
; MEMTOP with the end marker; else OUT OF MEMORY with nothing moved.
store:  lda #<PROGRAM
        sta POSP
        lda #>PROGRAM
        sta POSP+1
        lda #0
        sta OLDLEN
@find:  ldy #0
        lda (POSP),y
        sta T0
        iny
        lda (POSP),y
        sta T0+1
        ora T0
        beq @room
        lda T0          ; n against LNUM
        cmp LNUM
        lda T0+1
        sbc LNUM+1
        bcc @after
        lda T0
        cmp LNUM
        bne @room
        lda T0+1
        cmp LNUM+1
        bne @room
        ldy #2
        lda (POSP),y
        sta OLDLEN
        jmp @room
@after: ldy #2
        lda (POSP),y
        clc
        adc POSP
        sta POSP
        bcc @find
        inc POSP+1
        bne @find
@room:  sec             ; T0 = TOP - OLDLEN + NEWLEN + 2 must not pass MEMTOP
        lda TOP
        sbc OLDLEN
        sta T0
        lda TOP+1
        sbc #0
        sta T0+1
        clc
        lda T0
        adc NEWLEN
        sta T0
        lda T0+1
        adc #0
        sta T0+1
        clc
        lda T0
        adc #2
        sta T0
        lda T0+1
        adc #0
        sta T0+1
        lda MEMTOP
        cmp T0
        lda MEMTOP+1
        sbc T0+1
        bcs @fits
        lda #EMEMORY
        jmp error
@fits:  lda OLDLEN
        beq @insert
        clc             ; down: [POSP + OLDLEN, TOP + 2) to POSP
        lda POSP
        adc OLDLEN
        sta SRCP
        lda POSP+1
        adc #0
        sta SRCP+1
        lda POSP
        sta DSTP
        lda POSP+1
        sta DSTP+1
        jsr span        ; COUNT = TOP + 2 - SRCP
        jsr movedown
        sec
        lda TOP
        sbc OLDLEN
        sta TOP
        bcs @insert
        dec TOP+1
@insert:
        lda NEWLEN
        bne @open
        jmp loop
@open:  lda POSP        ; up: [POSP, TOP + 2) to POSP + NEWLEN, from the end
        sta SRCP
        lda POSP+1
        sta SRCP+1
        jsr span
        jsr moveup
        ldy #0
@copy:  lda DIRBUF,y
        sta (POSP),y
        iny
        cpy NEWLEN
        bne @copy
        clc
        lda TOP
        adc NEWLEN
        sta TOP
        bcc @done
        inc TOP+1
@done:  jmp loop

; COUNT = TOP + 2 - SRCP.
span:   sec
        lda TOP
        sbc SRCP
        sta COUNT
        lda TOP+1
        sbc SRCP+1
        sta COUNT+1
        clc
        lda COUNT
        adc #2
        sta COUNT
        bcc @done
        inc COUNT+1
@done:  rts

; COUNT bytes from SRCP to DSTP, ascending.
movedown:
        ldy #0
@loop:  lda COUNT
        ora COUNT+1
        beq @done
        lda (SRCP),y
        sta (DSTP),y
        inc SRCP
        bne @s
        inc SRCP+1
@s:     inc DSTP
        bne @d
        inc DSTP+1
@d:     lda COUNT
        bne @c
        dec COUNT+1
@c:     dec COUNT
        jmp @loop
@done:  rts

; COUNT bytes from SRCP up by NEWLEN, descending.
moveup: clc             ; SRCP to the last byte, DSTP NEWLEN above it
        lda SRCP
        adc COUNT
        sta SRCP
        lda SRCP+1
        adc COUNT+1
        sta SRCP+1
        lda SRCP
        bne @dec
        dec SRCP+1
@dec:   dec SRCP
        clc
        lda SRCP
        adc NEWLEN
        sta DSTP
        lda SRCP+1
        adc #0
        sta DSTP+1
        ldy #0
@loop:  lda COUNT
        ora COUNT+1
        beq @done
        lda (SRCP),y
        sta (DSTP),y
        lda SRCP
        bne @s
        dec SRCP+1
@s:     dec SRCP
        lda DSTP
        bne @d
        dec DSTP+1
@d:     dec DSTP
        lda COUNT
        bne @c
        dec COUNT+1
@c:     dec COUNT
        jmp @loop
@done:  rts

; UH1 ends here: the rest is in UH5.
        .org $C7FF
        .checksum $C000, $800
        .org $C800

; The record of line ACC: POSP, or NO SUCH LINE (BAD VALUE outside 1-9999, signed).
findline:
        lda ACC+1
        bmi @bad
        lda ACC
        ldx ACC+1
        jsr inrange
        bcs @bad
        lda #<PROGRAM
        sta POSP
        lda #>PROGRAM
        sta POSP+1
@next:  ldy #0
        lda (POSP),y
        iny
        ora (POSP),y
        beq @none
        dey
        lda (POSP),y
        cmp ACC
        bne @on
        iny
        lda (POSP),y
        cmp ACC+1
        beq @found
@on:    ldy #2
        lda (POSP),y
        clc
        adc POSP
        sta POSP
        bcc @next
        inc POSP+1
        bne @next
@none:  lda #ENOLINE
        jmp error
@found: rts
@bad:   lda #EVALUE
        jmp error

; Carry clear when A (low) X (high) is 1-9999, unsigned.
inrange:
        cpx #0
        bne @high
        cmp #0
        beq @out
        clc
        rts
@high:  cpx #>9999
        bcc @in
        bne @out
        cmp #<9999+1
        bcs @out
@in:    clc
        rts
@out:   sec
        rts

; LINBUF from X tokenised into DIRBUF+3, $00-terminated; NEWLEN the record's length.
tokenise:
        lda #3
        sta W
        lda #0
        sta QUOTE
@loop:  lda LINBUF,x
        beq @end
        ldy QUOTE
        bne @inquote
        cmp #'"'
        bne @word
        lda #1
        sta QUOTE
        lda #'"'
        jmp @copy
@inquote:
        cmp #'"'
        bne @copy
        lda #0
        sta QUOTE
        lda #'"'
        jmp @copy
@word:  jsr keyword
        bcc @plain
        ldy W
        sta DIRBUF,y
        inc W
        cmp #TREM
        bne @loop
@rest:  lda LINBUF,x
        beq @end
        ldy W
        sta DIRBUF,y
        inc W
        inx
        bne @rest
@plain: lda LINBUF,x
@copy:  ldy W
        sta DIRBUF,y
        inc W
        inx
        bne @loop
@end:   ldy W
        lda #0
        sta DIRBUF,y
        iny
        sty NEWLEN
        rts

; The keyword at LINBUF,X, if any: carry set, A its token, X past it.
keyword:
        stx KX
        lda #<words
        sta KP
        lda #>words
        sta KP+1
        lda #TPRINT
        sta KW
@try:   ldy #0
        ldx KX
@char:  lda (KP),y
        beq @none
        sta KC
        and #$7F
        cmp LINBUF,x
        bne @skip
        inx
        iny
        lda KC
        bpl @char
        lda KW
        sec
        rts
@skip:  lda (KP),y
        bmi @last
        iny
        bne @skip
@last:  iny
        tya
        clc
        adc KP
        sta KP
        bcc @n
        inc KP+1
@n:     inc KW
        jmp @try
@none:  ldx KX
        clc
        rts

; Prints byte A of a line as LIST shows it: a token as its keyword.
outtoken:
        cmp #TPRINT
        bcs @word
        jmp outc
@word:  sec
        sbc #TPRINT
        tax
        ldy #0
@find:  cpx #0
        beq @print
@skip:  lda words,y
        iny
        and #$80
        beq @skip
        dex
        jmp @find
@print: lda words,y
        pha
        and #$7F
        jsr outc
        iny
        pla
        bpl @print
        rts

; P5o's keywords in token order, each last letter with bit 7 set; 0 ends the table.
words:  .byte "PRIN", 'T'|$80, "INPU", 'T'|$80, "LE", 'T'|$80, "I", 'F'|$80
        .byte "GOT", 'O'|$80, "GOSU", 'B'|$80, "RETUR", 'N'|$80, "FO", 'R'|$80
        .byte "NEX", 'T'|$80, "EN", 'D'|$80, "RE", 'M'|$80, "POK", 'E'|$80
        .byte "CL", 'S'|$80, "LIS", 'T'|$80, "RU", 'N'|$80, "NE", 'W'|$80
        .byte "THE", 'N'|$80, "T", 'O'|$80, "STE", 'P'|$80, "AN", 'D'|$80
        .byte "O", 'R'|$80, "NO", 'T'|$80, "PEE", 'K'|$80, "RN", 'D'|$80
        .byte "AB", 'S'|$80, "US", 'R'|$80, 0

; --- Expressions (design §6.6), P5o's eight levels from the loosest -----------------------------

expr:   lda #0
        sta DEPTH
orlv:   jsr andlv
@loop:  jsr peek
        cmp #TOR
        bne @done
        jsr adv
        jsr pushacc
        jsr andlv
        jsr pullarg
        lda ARG
        ora ACC
        sta ACC
        lda ARG+1
        ora ACC+1
        sta ACC+1
        jmp @loop
@done:  rts

andlv:  jsr notlv
@loop:  jsr peek
        cmp #TAND
        bne @done
        jsr adv
        jsr pushacc
        jsr notlv
        jsr pullarg
        lda ARG
        and ACC
        sta ACC
        lda ARG+1
        and ACC+1
        sta ACC+1
        jmp @loop
@done:  rts

notlv:  jsr peek
        cmp #TNOT
        bne cmplv
        jsr adv
        jsr deeper
        jsr notlv
        lda ACC
        eor #$FF
        sta ACC
        lda ACC+1
        eor #$FF
        sta ACC+1
        dec DEPTH
        rts

; Comparisons, left to right: true -1, false 0. The operator as the results it accepts: bit 0
; less, bit 1 equal, bit 2 greater. The second character follows with no space.
cmplv:  jsr addlv
@loop:  jsr peek
        cmp #'='
        bne @less
        lda #2
        jmp @op
@less:  cmp #'<'
        bne @greater
        jsr adv
        ldy #0
        lda (TXT),y
        cmp #'>'
        bne @le
        lda #5
        jmp @op
@le:    cmp #'='
        bne @lt
        lda #3
        jmp @op
@lt:    lda #1
        jmp @op1
@greater:
        cmp #'>'
        bne @done
        jsr adv
        ldy #0
        lda (TXT),y
        cmp #'='
        bne @gt
        lda #6
        jmp @op
@gt:    lda #4
        jmp @op1
@done:  rts
@op:    jsr adv         ; past the operator's last character
@op1:   pha
        jsr pushacc
        jsr addlv
        jsr pullarg
        jsr scmp        ; ARG against ACC
        tax
        lda #1
        cpx #$FF
        beq @bit
        lda #2
        cpx #0
        beq @bit
        lda #4
@bit:   sta KC
        pla
        and KC
        beq @false
        lda #$FF
        bne @set
@false: lda #0
@set:   sta ACC
        sta ACC+1
        jmp @loop

addlv:  jsr mullv
@loop:  jsr peek
        cmp #'+'
        bne @minus
        jsr adv
        jsr pushacc
        jsr mullv
        jsr pullarg
        clc
        lda ARG
        adc ACC
        sta ACC
        lda ARG+1
        adc ACC+1
        sta ACC+1
        jmp @loop
@minus: cmp #'-'
        bne @done
        jsr adv
        jsr pushacc
        jsr mullv
        jsr pullarg
        sec
        lda ARG
        sbc ACC
        sta ACC
        lda ARG+1
        sbc ACC+1
        sta ACC+1
        jmp @loop
@done:  rts

mullv:  jsr unary
@loop:  jsr peek
        cmp #'*'
        bne @div
        jsr adv
        jsr pushacc
        jsr unary
        jsr pullarg
        jsr multiply
        jmp @loop
@div:   cmp #'/'
        bne @done
        jsr adv
        jsr pushacc
        jsr unary
        jsr pullarg
        jsr divide
        jmp @loop
@done:  rts

unary:  jsr peek
        cmp #'-'
        bne @plus
        jsr adv
        jsr deeper
        jsr unary
        jsr negate
        dec DEPTH
        rts
@plus:  cmp #'+'
        bne primary
        jsr adv
        jsr deeper
        jsr unary
        dec DEPTH
        rts

primary:
        jsr peek
        cmp #0
        beq @bad
        cmp #'0'
        bcc @notdigit
        cmp #'9'+1
        bcs @notdigit
        jmp number
@notdigit:
        cmp #'A'
        bcc @notvar
        cmp #'Z'+1
        bcs @notvar
        jsr adv
        sec
        sbc #'A'
        asl
        tax
        lda VARS,x
        sta ACC
        lda VARS+1,x
        sta ACC+1
        rts
@notvar:
        cmp #'('
        bne @function
        jmp paren
@function:
        jsr adv
        cmp #TPEEK
        beq @peek
        cmp #TRND
        beq @rnd
        cmp #TABS
        beq @abs
        cmp #TUSR
        beq @usr
@bad:   jmp syntax
@peek:  jsr paren
        lda ACC
        sta T0
        lda ACC+1
        sta T0+1
        ldy #0
        lda (T0),y
        sta ACC
        sty ACC+1
        rts
@abs:   jsr paren
        lda ACC+1
        bpl @pos
        jsr negate
@pos:   rts
@usr:   jsr paren
        jsr usrcall
        sta ACC
        sty ACC+1
        cld
        rts
@rnd:   jsr paren
        lda ACC+1
        bmi @badvalue
        ora ACC
        beq @badvalue
        jsr rndstep
        lda ACC         ; R mod N, unsigned
        sta T1
        lda ACC+1
        sta T1+1
        lda SEED
        sta T0
        lda SEED+1
        sta T0+1
        jsr udivide
        lda REM_
        sta ACC
        lda REM_+1
        sta ACC+1
        rts
@badvalue:
        lda #EVALUE
        jmp error

usrcall:
        jmp (ACC)

; ( expr ): one deeper.
paren:  lda #'('
        jsr expect
        jsr deeper
        jsr orlv
        lda #')'
        jsr expect
        dec DEPTH
        rts

deeper: inc DEPTH
        lda DEPTH
        cmp #7
        bcs @deep
        rts
@deep:  lda #EDEEP
        jmp error

; A literal: a run of digits, 0-65535 (else BAD VALUE), into ACC; SYNTAX with no digit.
number: jsr peek
        cmp #'0'
        bcc @bad
        cmp #'9'+1
        bcs @bad
        lda #0
        sta ACC
        sta ACC+1
@digit: ldy #0
        lda (TXT),y
        cmp #'0'
        bcc @done
        cmp #'9'+1
        bcs @done
        and #$0F
        jsr times10
        bcs @value
        jsr adv
        jmp @digit
@done:  rts
@bad:   jmp syntax
@value: lda #EVALUE
        jmp error

; ACC = ACC * 10 + A; carry set past 65535.
times10:
        pha
        asl ACC
        rol ACC+1
        bcs @over
        lda ACC
        sta T1
        lda ACC+1
        sta T1+1
        asl ACC
        rol ACC+1
        bcs @over
        asl ACC
        rol ACC+1
        bcs @over
        clc
        lda ACC
        adc T1
        sta ACC
        lda ACC+1
        adc T1+1
        sta ACC+1
        bcs @over
        pla
        clc
        adc ACC
        sta ACC
        lda ACC+1
        adc #0
        sta ACC+1
        rts
@over:  pla
        sec
        rts

; The variable at TXT: its index in A; SYNTAX if none.
variable:
        jsr peek
        cmp #'A'
        bcc @bad
        cmp #'Z'+1
        bcs @bad
        jsr adv
        sec
        sbc #'A'
        rts
@bad:   jmp syntax

; Variable X = ACC.
setvar: txa
        asl
        tax
        lda ACC
        sta VARS,x
        lda ACC+1
        sta VARS+1,x
        rts

pushacc:
        pla             ; this routine's return address under the value
        sta T0
        pla
        sta T0+1
        lda ACC+1
        pha
        lda ACC
        pha
        lda T0+1
        pha
        lda T0
        pha
        rts

pullarg:
        pla
        sta T0
        pla
        sta T0+1
        pla
        sta ARG
        pla
        sta ARG+1
        lda T0+1
        pha
        lda T0
        pha
        rts

negate: sec
        lda #0
        sbc ACC
        sta ACC
        lda #0
        sbc ACC+1
        sta ACC+1
        rts

; ARG against ACC, signed: A = $FF less, 0 equal, 1 greater.
scmp:   lda ARG
        cmp ACC
        bne @differ
        lda ARG+1
        cmp ACC+1
        bne @differ
        lda #0
        rts
@differ:
        lda ARG
        cmp ACC
        lda ARG+1
        sbc ACC+1
        bvc @nv
        eor #$80
@nv:    bmi @less
        lda #1
        rts
@less:  lda #$FF
        rts

; ACC = ARG * ACC, the low 16 bits.
multiply:
        lda ARG
        sta T0
        lda ARG+1
        sta T0+1
        lda ACC
        sta T1
        lda ACC+1
        sta T1+1
        lda #0
        sta ACC
        sta ACC+1
        ldx #16
@loop:  lsr T1+1
        ror T1
        bcc @shift
        clc
        lda ACC
        adc T0
        sta ACC
        lda ACC+1
        adc T0+1
        sta ACC+1
@shift: asl T0
        rol T0+1
        dex
        bne @loop
        rts

; ACC = ARG / ACC, signed, toward zero; DIVIDE BY ZERO.
divide: lda ACC
        ora ACC+1
        bne @ok
        lda #EDIVIDE
        jmp error
@ok:    lda ARG+1
        eor ACC+1
        sta SIGN
        lda ACC+1
        bpl @divisor
        jsr negate
@divisor:
        lda ACC
        sta T1
        lda ACC+1
        sta T1+1
        lda ARG
        sta ACC
        lda ARG+1
        sta ACC+1
        bpl @dividend
        jsr negate
@dividend:
        lda ACC
        sta T0
        lda ACC+1
        sta T0+1
        jsr udivide
        lda T0
        sta ACC
        lda T0+1
        sta ACC+1
        lda SIGN
        bpl @done
        jsr negate
@done:  rts

; T0 = T0 / T1 unsigned, REM_ the remainder.
udivide:
        lda #0
        sta REM_
        sta REM_+1
        ldx #16
@loop:  asl T0
        rol T0+1
        rol REM_
        rol REM_+1
        sec
        lda REM_
        sbc T1
        tay
        lda REM_+1
        sbc T1+1
        bcc @next
        sta REM_+1
        sty REM_
        inc T0
@next:  dex
        bne @loop
        rts

; P5o's xorshift: R ^= R << 7, R ^= R >> 9, R ^= R << 8.
rndstep:
        lda SEED
        sta T0
        lda SEED+1
        sta T0+1
        ldx #7
@shl:   asl T0
        rol T0+1
        dex
        bne @shl
        lda SEED
        eor T0
        sta SEED
        lda SEED+1
        eor T0+1
        sta SEED+1
        lda SEED+1      ; >> 9: the high byte >> 1
        lsr
        eor SEED
        sta SEED
        lda SEED        ; << 8: the low byte into the high
        eor SEED+1
        sta SEED+1
        rts

; --- Reading the line ------------------------------------------------------------------------------

; Spaces skipped; A the byte at TXT (0 at the line's end), not taken.
peek:   ldy #0
@loop:  lda (TXT),y
        cmp #' '
        bne @done
        jsr adv
        jmp @loop
@done:  rts

adv:    inc TXT
        bne @done
        inc TXT+1
@done:  rts

; The byte A next, after spaces, else SYNTAX.
expect: sta KC
        jsr peek
        cmp KC
        bne @bad
        jmp adv
@bad:   jmp syntax

; --- Output, with BASIC's column -------------------------------------------------------------------

outc:   jsr EDPRINT
        cmp #$0D
        beq @zero
        cmp #$0C
        beq @zero
        inc COL
        pha
        lda COL
        cmp #COLUMNS
        bne @kept
        lda #0
        sta COL
@kept:  pla
        rts
@zero:  pha
        lda #0
        sta COL
        pla
        rts

newline:
        lda #$0D
        jmp outc

fresh:  lda COL
        beq @done
        jmp newline
@done:  rts

printacc:
        lda ACC
        ldy ACC+1
numout: sta NUM
        sty NUM+1
        tya
        bpl @pos
        lda #'-'
        jsr outc
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
        jsr outc
        lda #1
        sta NZ
@skip:  inx
        cpx #4
        bne @power
        lda NUM
        ora #'0'
        jmp outc

powlo:  .byte <10000, <1000, <100, <10
powhi:  .byte >10000, >1000, >100, >10

; --- Errors -----------------------------------------------------------------------------------------

syntax: lda #ESYNTAX
error:  sta ECODE
        ldx ERRSP
        txs
        jsr fresh
        ldx ECODE
        lda msglo-1,x
        sta T0
        lda msghi-1,x
        sta T0+1
        ldy #0
@msg:   lda (T0),y
        beq @word
        jsr outc
        iny
        bne @msg
@word:  ldx #0
@e:     lda errortext,x
        beq @where
        jsr outc
        inx
        bne @e
@where: jsr inline
        jsr newline
        jmp ready

dobreak:
        ldx ERRSP
        txs
        jsr fresh
        ldx #0
@b:     lda breaktext,x
        beq @where
        jsr outc
        inx
        bne @b
@where: jsr inline
        jsr newline
        jmp ready

; " IN n" when a program line was running.
inline: lda CURLINE
        ora CURLINE+1
        beq @done
        ldx #0
@in:    lda intext,x
        beq @number
        jsr outc
        inx
        bne @in
@number:
        ldy #0
        lda (CURLINE),y
        pha
        iny
        lda (CURLINE),y
        tay
        pla
        jmp numout
@done:  rts

errortext:
        .byte " ERROR", 0
breaktext:
        .byte "BREAK", 0
intext: .byte " IN ", 0

msglo:  .byte <m1, <m2, <m3, <m4, <m5, <m6, <m7, <m8, <m9
msghi:  .byte >m1, >m2, >m3, >m4, >m5, >m6, >m7, >m8, >m9
m1:     .byte "SYNTAX", 0
m2:     .byte "NO SUCH LINE", 0
m3:     .byte "NO GOSUB", 0
m4:     .byte "NO FOR", 0
m5:     .byte "TOO DEEP", 0
m6:     .byte "DIVIDE BY ZERO", 0
m7:     .byte "BAD VALUE", 0
m8:     .byte "OUT OF MEMORY", 0
m9:     .byte "ILLEGAL DIRECT", 0

; --- NEW and RUN's clearing ---------------------------------------------------------------------------

new:    lda #<PROGRAM
        sta TOP
        lda #>PROGRAM
        sta TOP+1
        lda #0
        sta PROGRAM
        sta PROGRAM+1
clear:  lda #0
        ldx #2*26-1
@vars:  sta VARS,x
        dex
        bpl @vars
        sta CSP
        sta SEED+1
        lda #1
        sta SEED
        rts

        .org $CFFF
        .checksum $C800, $800
