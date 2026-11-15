#!/usr/bin/env python3
"""Reference interpreter of Bodgewire's Tiny BASIC, and its conformance runner.

The language is docs/p5o-tiny-basic/TINY_BASIC.md; this file follows it as written, section
by section, and is not a model of either ROM. A difference found on a ROM is a question for the
spec.

Usage:
  tiny_basic.py check [--width W ...] [--ram N] [--mutate] FILE...
  tiny_basic.py run [--width W]

check runs each session (tests/data/basic/*.txt) at each width (default 32 and 40) and compares
the screen with its rows; --mutate changes each session's last row and expects every session to
fail. run is an interactive prompt on the terminal (Ctrl-C is the break key).
"""

import argparse
import pathlib
import re
import signal
import sys

# --- The program in RAM: tokens -------------------------------------------------------------

KEYWORDS = [
    "PRINT", "INPUT", "LET", "IF", "GOTO", "GOSUB", "RETURN", "FOR",
    "NEXT", "END", "REM", "POKE", "CLS", "LIST", "RUN", "NEW",
    "THEN", "TO", "STEP", "AND", "OR", "NOT", "PEEK", "RND", "ABS", "USR",
]
TOKEN = {word: 0x80 + i for i, word in enumerate(KEYWORDS)}
WORD = {code: word for word, code in TOKEN.items()}
STATEMENTS = set(KEYWORDS[:16])

T_PRINT, T_INPUT, T_LET, T_IF, T_GOTO, T_GOSUB, T_RETURN, T_FOR = range(0x80, 0x88)
T_NEXT, T_END, T_REM, T_POKE, T_CLS, T_LIST, T_RUN, T_NEW = range(0x88, 0x90)
T_THEN, T_TO, T_STEP, T_AND, T_OR, T_NOT, T_PEEK, T_RND, T_ABS, T_USR = range(0x90, 0x9A)

# The spec: no keyword is the beginning of another one, so matching in table order is greedy.
assert not any(a != b and b.startswith(a) for a in KEYWORDS for b in KEYWORDS)

LINE_MAX = 64
STACK_MAX = 8
DEPTH_MAX = 6
QUOTE, COLON, SPACE = ord('"'), ord(":"), ord(" ")

MESSAGES = {
    1: "SYNTAX",
    2: "NO SUCH LINE",
    3: "NO GOSUB",
    4: "NO FOR",
    5: "TOO DEEP",
    6: "DIVIDE BY ZERO",
    7: "BAD VALUE",
    8: "OUT OF MEMORY",
    9: "ILLEGAL DIRECT",
}
SYNTAX, NO_SUCH_LINE, NO_GOSUB, NO_FOR, TOO_DEEP, DIVIDE, BAD_VALUE, OUT_OF_MEMORY, DIRECT = (
    range(1, 10))


def tokenise(text):
    """A line's text (after its number) as stored: keywords as tokens outside quotes and REM."""
    out = bytearray()
    i = 0
    quoted = False
    while i < len(text):
        c = text[i]
        if c == '"':
            quoted = not quoted
        elif not quoted:
            word = next((w for w in KEYWORDS if text.startswith(w, i)), None)
            if word:
                out.append(TOKEN[word])
                i += len(word)
                if word == "REM":
                    out += text[i:].encode("ascii")
                    break
                continue
        out.append(ord(c))
        i += 1
    return bytes(out)


def detokenise(data):
    return "".join(WORD.get(b, chr(b)) for b in data)


class BasicError(Exception):
    def __init__(self, code):
        super().__init__(MESSAGES[code])
        self.code = code


class Break(Exception):
    pass


class Program:
    """The program area: records of line number (low first), record length, text, $00; then
    two $00 bytes."""

    def __init__(self, size):
        self.ram = bytearray(size)
        self.new()

    def new(self):
        self.ram[0:2] = b"\0\0"

    def records(self):
        """(number, text bytes) for each line, read from the bytes."""
        at = 0
        while True:
            number = self.ram[at] | self.ram[at + 1] << 8
            if number == 0:
                return
            length = self.ram[at + 2]
            yield number, bytes(self.ram[at + 3:at + length - 1])
            at += length

    def find(self, number):
        return next((text for n, text in self.records() if n == number), None)

    def first(self):
        return next(self.records(), (None, None))

    def after(self, number):
        return next(((n, t) for n, t in self.records() if n > number), (None, None))

    def store(self, number, text):
        lines = dict(self.records())
        if text is None:
            lines.pop(number, None)
        else:
            lines[number] = text
        image = bytearray()
        for n in sorted(lines):
            body = lines[n]
            image += bytes([n & 0xFF, n >> 8, 3 + len(body) + 1]) + body + b"\0"
        image += b"\0\0"
        if len(image) > len(self.ram):
            raise BasicError(OUT_OF_MEMORY)
        self.ram[0:len(image)] = image


def wrap16(value):
    return (value + 0x8000) % 0x10000 - 0x8000


def rnd_step(s):
    s ^= (s << 7) & 0xFFFF
    s ^= s >> 9
    s ^= (s << 8) & 0xFFFF
    return s


# --- The interpreter ------------------------------------------------------------------------

class Basic:
    """One machine's BASIC. The editor contract is print_char, read_line and break_pressed,
    given by the caller; enter() is the editor handing BASIC a line."""

    def __init__(self, width, print_char, read_line, break_pressed, program_size=7000):
        self.width = width
        self.print_char = print_char
        self.read_line = read_line
        self.break_pressed = break_pressed
        self.program = Program(program_size)
        self.memory = bytearray(0x10000)
        self.column = 0
        self.new()

    # Output, with BASIC's own column count.
    def out(self, text):
        for c in text:
            self.print_char(c)
            self.column = (self.column + 1) % self.width

    def newline(self):
        self.print_char("\r")
        self.column = 0

    def fresh_line(self):
        if self.column != 0:
            self.newline()

    def ready(self):
        self.fresh_line()
        self.out("READY")
        self.newline()

    def power_on(self):
        self.new()
        self.ready()

    def new(self):
        self.program.new()
        self.clear()

    def clear(self):
        self.variables = [0] * 26
        self.seed = 1
        self.stack = []

    # The editor hands BASIC one line.
    def enter(self, line):
        self.column = 0
        self.line = None
        match = None
        assert len(line) <= LINE_MAX and all(0x20 <= ord(c) <= 0x5F for c in line)
        line = line.lstrip(" ")
        try:
            match = re.match(r"([0-9]+) *", line)
            if match:
                number = int(match.group(1))
                if not 1 <= number <= 9999:
                    raise BasicError(BAD_VALUE)
                text = line[match.end():]
                self.program.store(number, tokenise(text) if text else None)
                return
            self.stack = []
            self.direct_text = tokenise(line)
            self.run(None, self.direct_text, 0)
        except BasicError as error:
            self.fresh_line()
            self.out(MESSAGES[error.code] + " ERROR")
            if self.line is not None and not match:
                self.out(f" IN {self.line}")
            self.newline()
        except Break:
            self.fresh_line()
            self.out("BREAK" + (f" IN {self.line}" if self.line is not None else ""))
            self.newline()
        self.ready()

    # Running: self.line is the line number (None for the direct line), self.text its bytes,
    # self.at the position in it.
    def run(self, line, text, at):
        self.line, self.text, self.at = line, text, at
        while self.text is not None:
            if self.break_pressed():
                raise Break()
            self.jumped = False
            self.statement()
            if not self.jumped:
                self.end_of_statement()

    def end_of_statement(self):
        """After a statement: ':' goes on in the line, its end goes to the next line."""
        if self.text is None:
            return
        self.skip()
        if self.peek() == COLON:
            self.at += 1
        elif self.peek() is None:
            self.next_line()
        else:
            raise BasicError(SYNTAX)

    def next_line(self):
        if self.line is None:
            self.text = None
        else:
            self.line, self.text = self.program.after(self.line)
            self.at = 0

    def stop(self):
        self.text = None

    def goto(self, number):
        if not 1 <= number <= 9999:
            raise BasicError(BAD_VALUE)
        text = self.program.find(number)
        if text is None:
            raise BasicError(NO_SUCH_LINE)
        if self.line is None:
            self.stack = []
        self.line, self.text, self.at = number, text, 0
        self.jumped = True

    def resume(self, line, at):
        """Back to a stack entry's place: just after its statement."""
        self.line, self.at = line, at
        self.text = self.direct_text if line is None else self.program.find(line)

    # Reading the line.
    def peek(self):
        return self.text[self.at] if self.at < len(self.text) and self.text[self.at] else None

    def skip(self):
        while self.peek() == SPACE:
            self.at += 1

    def accept(self, code):
        self.skip()
        if self.peek() == code:
            self.at += 1
            return True
        return False

    def expect(self, code):
        if not self.accept(code):
            raise BasicError(SYNTAX)

    def at_end(self):
        self.skip()
        return self.peek() in (None, COLON)

    def variable(self):
        self.skip()
        c = self.peek()
        if c is None or not ord("A") <= c <= ord("Z"):
            raise BasicError(SYNTAX)
        self.at += 1
        return c - ord("A")

    def number(self):
        """A literal: a run of digits, 0 to 65535, kept as its low 16 bits."""
        self.skip()
        start = self.at
        while self.peek() is not None and ord("0") <= self.peek() <= ord("9"):
            self.at += 1
        if self.at == start:
            raise BasicError(SYNTAX)
        value = int(self.text[start:self.at])
        if value > 65535:
            raise BasicError(BAD_VALUE)
        return value

    # Expressions, the spec's eight levels from the loosest.
    def expression(self):
        self.depth = 0
        return self.or_level()

    def deeper(self):
        """An open parenthesis or a unary operator waiting for its operand."""
        self.depth += 1
        if self.depth > DEPTH_MAX:
            raise BasicError(TOO_DEEP)

    def or_level(self):
        value = self.and_level()
        while self.accept(T_OR):
            value = wrap16(value | self.and_level())
        return value

    def and_level(self):
        value = self.not_level()
        while self.accept(T_AND):
            value = wrap16(value & self.not_level())
        return value

    def not_level(self):
        if self.accept(T_NOT):
            self.deeper()
            value = wrap16(~self.not_level())
            self.depth -= 1
            return value
        return self.compare_level()

    def compare_level(self):
        value = self.add_level()
        while True:
            self.skip()
            c = self.peek()
            if c == ord("="):
                self.at += 1
                test = lambda a, b: a == b
            elif c == ord("<"):
                self.at += 1
                if self.peek() == ord(">"):
                    self.at += 1
                    test = lambda a, b: a != b
                elif self.peek() == ord("="):
                    self.at += 1
                    test = lambda a, b: a <= b
                else:
                    test = lambda a, b: a < b
            elif c == ord(">"):
                self.at += 1
                if self.peek() == ord("="):
                    self.at += 1
                    test = lambda a, b: a >= b
                else:
                    test = lambda a, b: a > b
            else:
                return value
            value = -1 if test(value, self.add_level()) else 0

    def add_level(self):
        value = self.mul_level()
        while True:
            if self.accept(ord("+")):
                value = wrap16(value + self.mul_level())
            elif self.accept(ord("-")):
                value = wrap16(value - self.mul_level())
            else:
                return value

    def mul_level(self):
        value = self.unary_level()
        while True:
            if self.accept(ord("*")):
                value = wrap16(value * self.unary_level())
            elif self.accept(ord("/")):
                divisor = self.unary_level()
                if divisor == 0:
                    raise BasicError(DIVIDE)
                quotient = abs(value) // abs(divisor)
                value = wrap16(quotient if (value < 0) == (divisor < 0) else -quotient)
            else:
                return value

    def unary_level(self):
        for sign in (-1, 1):
            if self.accept(ord("-" if sign < 0 else "+")):
                self.deeper()
                value = wrap16(sign * self.unary_level())
                self.depth -= 1
                return value
        return self.primary()

    def parenthesised(self):
        self.expect(ord("("))
        self.deeper()
        value = self.or_level()
        self.expect(ord(")"))
        self.depth -= 1
        return value

    def primary(self):
        self.skip()
        c = self.peek()
        if c is None:
            raise BasicError(SYNTAX)
        if ord("0") <= c <= ord("9"):
            return wrap16(self.number())
        if ord("A") <= c <= ord("Z"):
            self.at += 1
            return self.variables[c - ord("A")]
        if c == ord("("):
            return self.parenthesised()
        self.at += 1
        if c == T_PEEK:
            return self.memory[self.parenthesised() & 0xFFFF]
        if c == T_RND:
            n = self.parenthesised()
            if n <= 0:
                raise BasicError(BAD_VALUE)
            self.seed = rnd_step(self.seed)
            return self.seed % n
        if c == T_ABS:
            return wrap16(abs(self.parenthesised()))
        if c == T_USR:
            raise NotImplementedError("USR depends on the Machine")
        raise BasicError(SYNTAX)

    # Statements.
    def statement(self):
        self.skip()
        c = self.peek()
        if c is None or c == COLON:
            return  # an empty statement
        if ord("A") <= c <= ord("Z"):
            return self.let()
        self.at += 1
        handler = {
            T_PRINT: self.print_, T_INPUT: self.input_, T_LET: self.let, T_IF: self.if_,
            T_GOTO: self.goto_, T_GOSUB: self.gosub, T_RETURN: self.return_, T_FOR: self.for_,
            T_NEXT: self.next_, T_END: self.stop, T_REM: self.rem, T_POKE: self.poke,
            T_CLS: self.cls, T_LIST: self.list_, T_RUN: self.run_, T_NEW: self.new_,
        }.get(c)
        if handler is None:
            raise BasicError(SYNTAX)
        handler()

    def print_(self):
        newline = True
        while not self.at_end():
            if self.accept(ord(";")):
                newline = False
                continue
            if self.accept(ord(",")):
                self.out(" " * (8 - self.column % 8))
                newline = False
                continue
            if self.peek() == QUOTE:
                self.at += 1
                start = self.at
                while self.peek() not in (None, QUOTE):
                    self.at += 1
                self.out(self.text[start:self.at].decode("ascii"))
                self.accept(QUOTE)
            else:
                self.out(str(self.expression()))
            newline = True
            self.skip()
            if self.peek() not in (None, COLON, ord(";"), ord(",")):
                raise BasicError(SYNTAX)
        if newline:
            self.newline()

    def input_(self):
        if self.line is None:
            raise BasicError(DIRECT)
        prompt = ""
        self.skip()
        if self.peek() == QUOTE:
            self.at += 1
            start = self.at
            while self.peek() not in (None, QUOTE):
                self.at += 1
            prompt = self.text[start:self.at].decode("ascii")
            self.expect(QUOTE)
            self.expect(ord(";"))
        while True:
            var = self.variable()
            while True:
                self.out(prompt + "? ")
                answer = self.read_line()
                if answer is BREAK:
                    raise Break()
                self.column = 0
                match = re.fullmatch(r" *([+-]?)([0-9]{1,5}) *", answer)
                if match and int(match.group(2)) <= 65535:
                    value = int(match.group(2))
                    self.variables[var] = wrap16(-value if match.group(1) == "-" else value)
                    break
                self.out("REDO")
                self.newline()
            prompt = ""
            if not self.accept(ord(",")):
                return

    def let(self):
        var = self.variable()
        self.expect(ord("="))
        self.variables[var] = self.expression()

    def if_(self):
        condition = self.expression()
        self.expect(T_THEN)
        self.skip()
        if condition == 0:
            self.next_line()
            self.jumped = True
        elif self.peek() is not None and ord("0") <= self.peek() <= ord("9"):
            self.goto(wrap16(self.number()))
        else:
            self.statement()

    def goto_(self):
        self.goto(self.expression())

    def gosub(self):
        if self.line is None:
            raise BasicError(DIRECT)
        number = self.expression()
        self.push(("GOSUB", self.line, self.at))
        self.goto(number)

    def return_(self):
        while self.stack and self.stack[-1][0] == "FOR":
            self.stack.pop()
        if not self.stack:
            raise BasicError(NO_GOSUB)
        _, line, at = self.stack.pop()
        self.resume(line, at)

    def push(self, entry):
        if len(self.stack) >= STACK_MAX:
            raise BasicError(TOO_DEEP)
        self.stack.append(entry)

    def find_for(self, var):
        """Index of the innermost FOR entry (of var, if given) above the innermost GOSUB."""
        for i in range(len(self.stack) - 1, -1, -1):
            entry = self.stack[i]
            if entry[0] == "GOSUB":
                return None
            if var is None or entry[1] == var:
                return i
        return None

    def for_(self):
        var = self.variable()
        self.expect(ord("="))
        start = self.expression()
        self.expect(T_TO)
        limit = self.expression()
        step = self.expression() if self.accept(T_STEP) else 1
        self.variables[var] = start
        found = self.find_for(var)
        if found is not None:
            del self.stack[found:]
        self.push(("FOR", var, limit, step, self.line, self.at))

    def next_(self):
        var = None if self.at_end() else self.variable()
        found = self.find_for(var)
        if found is None:
            raise BasicError(NO_FOR)
        del self.stack[found + 1:]
        _, var, limit, step, line, at = self.stack[-1]
        total = self.variables[var] + step
        self.variables[var] = wrap16(total)
        overflow = total != wrap16(total)
        past = total > limit if step >= 0 else total < limit
        if overflow or past:
            self.stack.pop()
        else:
            self.resume(line, at)

    def rem(self):
        self.at = len(self.text)

    def poke(self):
        address = self.expression()
        self.expect(ord(","))
        self.memory[address & 0xFFFF] = self.expression() & 0xFF

    def cls(self):
        self.print_char("\f")
        self.column = 0

    def list_(self):
        first, last = 1, 9999
        if not self.at_end():
            first = last = self.number()
            if self.accept(ord("-")):
                last = self.number()
            if not (1 <= first <= 9999 and 1 <= last <= 9999):
                raise BasicError(BAD_VALUE)
        for number, text in self.program.records():
            if first <= number <= last:
                if self.break_pressed():
                    raise Break()
                self.out(f"{number} {detokenise(text)}")
                self.newline()

    def run_(self):
        self.clear()
        self.line, self.text = self.program.first()
        self.at = 0
        self.jumped = True

    def new_(self):
        self.new()
        self.stop()


BREAK = object()  # what read_line returns for a press of the break key


# --- Sessions -------------------------------------------------------------------------------

class Screen:
    """The output area. With echo, entered lines show where they were typed (the PET's
    editor); without, the editor types on a line of its own (a ZX80-style bottom line)."""

    def __init__(self, width, echo):
        self.width = width
        self.echo = echo
        self.clear()

    def clear(self):
        self.lines = [""]

    def put(self, c):
        if c == "\r":
            self.lines.append("")
        elif c == "\f":
            self.clear()
        else:
            assert 0x20 <= ord(c) <= 0x5F, repr(c)
            if len(self.lines[-1]) == self.width:
                self.lines.append("")
            self.lines[-1] += c

    def typed(self, text):
        if self.echo:
            for c in text:
                self.put(c)
            self.put("\r")
        elif self.lines[-1]:
            self.put("\r")

    def rows(self):
        rows = [row.rstrip() for row in self.lines]
        while rows and not rows[-1]:
            rows.pop()
        return rows


ROW_WIDTH = 31
ROW_COUNT = 21
RAM_DIGITS = 5
PROGRAM_SIZE = 256
TYPED_CHARACTERS = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \"$()*+,-./:;<=>?")
STATEMENT_LIMIT = 200_000
BREAK_AFTER = 1000


class SessionError(Exception):
    pass


class Session:
    """A conformance session: rows of what BASIC printed, '|', what the player typed."""

    def __init__(self, path, ram):
        self.path = path
        self.rows = []      # [printed, typed or None, break after it]
        self.needs_typing = False
        self.cleared = 0     # rows above a ~clear: typed, then wiped by CLS
        lines = pathlib.Path(path).read_text().splitlines()
        for number, raw in enumerate(lines, 1):
            where = f"{path}:{number}"
            if raw.startswith("~ ") or raw == "~":
                continue
            if raw == "~clear":
                self.cleared = len(self.rows)
                continue
            if raw == "~typed":
                self.needs_typing = True
                continue
            if raw == "~break":
                typed = [row for row in self.rows if row[1] is not None]
                if not typed:
                    raise SessionError(f"{where}: ~break before any typed line")
                typed[-1][2] = True
                continue
            if raw.startswith("~"):
                raise SessionError(f"{where}: unknown directive {raw!r}")
            printed, bar, typed = raw.partition("|")
            width = len((printed + typed).replace("{RAM}", "9" * RAM_DIGITS))
            if width > ROW_WIDTH:
                raise SessionError(f"{where}: row longer than {ROW_WIDTH}")
            printed, typed = printed.replace("{RAM}", str(ram)), typed.replace("{RAM}", str(ram))
            if any(not 0x20 <= ord(c) <= 0x5F for c in printed + typed):
                raise SessionError(f"{where}: a character outside $20-$5F")
            if bar and not set(typed) <= TYPED_CHARACTERS:
                raise SessionError(f"{where}: typed text with a key not on both Machines")
            self.rows.append([printed, typed if bar else None, False])
        if len(self.rows) > ROW_COUNT:
            raise SessionError(f"{path}: more than {ROW_COUNT} rows")

    def expected(self, echo):
        shown = self.rows[self.cleared:]
        if echo:
            rows = [p + (t or "") for p, t, _ in shown]
        else:
            rows = [p for p, t, _ in shown if p or t is None]
        rows = [row.rstrip() for row in rows]
        while rows and not rows[-1]:
            rows.pop()
        return rows

    def run(self, width, echo):
        screen = Screen(width, echo)
        typed = [(t, b) for _, t, b in self.rows if t is not None]
        state = {"break": False, "polls": 0, "statements": 0}

        def read_line():
            if state["break"]:
                state["break"] = False
                return BREAK
            if not typed:
                raise SessionError("BASIC asked for input with no typed line left")
            text, armed = typed.pop(0)
            screen.typed(text)
            state["break"], state["polls"] = armed, 0
            return text

        def break_pressed():
            state["statements"] += 1
            if state["statements"] > STATEMENT_LIMIT:
                raise SessionError("runs forever (no ~break)")
            if state["break"]:
                state["polls"] += 1
                if state["polls"] >= BREAK_AFTER:
                    state["break"] = False
                    return True
            return False

        basic = Basic(width, screen.put, read_line, break_pressed, PROGRAM_SIZE)
        basic.power_on()
        screen.typed("CLS")
        basic.enter("CLS")
        while typed:
            basic.enter(read_line())
            if state["break"]:
                raise SessionError("~break never reached (the line ended first)")
        return screen.rows()

    def check(self, width, echo, mutate=False):
        """None if the screen matches the view, or a message naming the first different row."""
        expected = self.expected(echo)
        if mutate:
            last = max(i for i, (p, _, _) in enumerate(self.rows) if p.strip())
            row = self.rows[last][0].rstrip()
            changed = row[:-1] + ("#" if row[-1] != "#" else "%")
            index = sum(1 for p, t, _ in self.rows[self.cleared:last] if echo or p or t is None)
            expected[index] = changed + (self.rows[last][1] or "" if echo else "")
        actual = self.run(width, echo)
        for i in range(max(len(expected), len(actual))):
            want = expected[i] if i < len(expected) else "(nothing)"
            got = actual[i] if i < len(actual) else "(nothing)"
            if want != got:
                view = "full" if echo else "printed"
                return f"{view} view, row {i}: expected {want!r}, got {got!r}"
        return None


def self_check():
    """The spec's layout example, the token table and the RND sequence."""
    program = Program(64)
    program.store(10, tokenise("PRINT A"))
    assert bytes(program.ram[:9]) == bytes.fromhex("0a000780204100" "0000"), program.ram[:9]
    assert TOKEN["PRINT"] == 0x80 and TOKEN["NEW"] == 0x8F and TOKEN["USR"] == 0x99
    assert tokenise("FORI=1TO9") == bytes([T_FOR]) + b"I=1" + bytes([T_TO]) + b"9"
    assert tokenise('PRINT "GOTO"') == bytes([T_PRINT]) + b' "GOTO"'
    assert tokenise("REM PRINT") == bytes([T_REM]) + b" PRINT"
    s, values = 1, []
    for _ in range(4):
        s = rnd_step(s)
        values.append(s % 1000)
    assert values == [153, 609, 801, 787], values


def check(args):
    self_check()
    widths = args.width or [32, 40]
    views = [True] if args.mutate else [True, False]
    failed = 0
    for path in args.files:
        try:
            session = Session(path, args.ram)
            results = [(f"width {w}", session.check(w, echo, args.mutate))
                       for w in widths for echo in views]
        except SessionError as error:
            results = [("", str(error))]
        if args.mutate:
            caught = all(message for _, message in results)
            print(f"{'ok  ' if caught else 'FAIL'} {path} (mutated)")
            failed += not caught
            continue
        bad = [(where, m) for where, m in results if m]
        for where, message in bad:
            print(f"FAIL {path} {where}: {message}")
        if not bad:
            print(f"ok   {path}")
        failed += bool(bad)
    print(f"{len(args.files) - failed}/{len(args.files)} sessions"
          + (" caught" if args.mutate else " passed"))
    return 1 if failed else 0


def repl(args):
    pressed = {"break": False}
    signal.signal(signal.SIGINT, lambda *_: pressed.update({"break": True}))

    def put(c):
        sys.stdout.write("\n" if c == "\r" else "\x1b[2J\x1b[H" if c == "\f" else c)
        sys.stdout.flush()

    def read_line():
        try:
            text = input()
        except (EOFError, KeyboardInterrupt):
            return BREAK
        if pressed["break"]:
            pressed["break"] = False
            return BREAK
        return "".join(c for c in text.upper() if 0x20 <= ord(c) <= 0x5F)[:LINE_MAX].rstrip()

    def break_pressed():
        hit, pressed["break"] = pressed["break"], False
        return hit

    basic = Basic(args.width, put, read_line, break_pressed)
    basic.power_on()
    while True:
        try:
            line = input()
        except EOFError:
            return 0
        except KeyboardInterrupt:
            print()
            continue
        basic.enter("".join(c for c in line.upper() if 0x20 <= ord(c) <= 0x5F)[:LINE_MAX]
                    .rstrip())


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    c = commands.add_parser("check")
    c.add_argument("--width", type=int, action="append")
    c.add_argument("--ram", type=int, default=16384)
    c.add_argument("--mutate", action="store_true")
    c.add_argument("files", nargs="*")
    r = commands.add_parser("run")
    r.add_argument("--width", type=int, default=40)
    args = parser.parse_args()
    return check(args) if args.command == "check" else repl(args)


if __name__ == "__main__":
    sys.exit(main())
