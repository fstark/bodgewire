#include "parts/z80_core.hpp"

#include <array>
#include <utility>

namespace bw {

namespace {

constexpr std::uint8_t kC = 0x01, kN = 0x02, kPv = 0x04, kX = 0x08, kH = 0x10, kY = 0x20, kZ = 0x40,
                       kS = 0x80;
constexpr std::uint8_t kXy = kX | kY;

constexpr std::array<std::uint8_t, 256> kParity = [] {
    std::array<std::uint8_t, 256> table{};
    for (unsigned v = 0; v < 256; ++v) {
        unsigned ones = 0;
        for (unsigned b = 0; b < 8; ++b) {
            ones += (v >> b) & 1;
        }
        table[v] = (ones & 1) == 0 ? kPv : 0;
    }
    return table;
}();

constexpr std::uint8_t Parity(unsigned value) { return kParity[value & 0xFF]; }

// S, Z, Y and X from a result byte.
constexpr std::uint8_t Szxy(std::uint8_t value) {
    return static_cast<std::uint8_t>((value & (kS | kXy)) | (value == 0 ? kZ : 0));
}

constexpr std::uint8_t Lo(unsigned value) { return static_cast<std::uint8_t>(value); }
constexpr std::uint8_t Hi(unsigned value) { return static_cast<std::uint8_t>(value >> 8); }
constexpr std::uint16_t Word(unsigned hi, unsigned lo) {
    return static_cast<std::uint16_t>((hi & 0xFF) << 8 | (lo & 0xFF));
}

// Opcode fields (Cristian Dinu, "Decoding Z80 Opcodes"): x = bits 7-6, y = 5-3, z = 2-0,
// p = 5-4, q = 3.
constexpr int X(std::uint8_t op) { return op >> 6; }
constexpr int Y(std::uint8_t op) { return (op >> 3) & 7; }
constexpr int Z(std::uint8_t op) { return op & 7; }
constexpr int P(std::uint8_t op) { return (op >> 4) & 3; }
constexpr int Q(std::uint8_t op) { return (op >> 3) & 1; }

enum Resume : int { kStart = -1 };

}  // namespace

// One M-cycle: set it up, return to the caller, and resume after it when step() is called with
// its data (data_). The resume points are case labels of run()'s switch, numbered by
// __COUNTER__; they are never inside a nested switch (decoding uses if/else and goto).
#define Z80_CYCLE_AT(setup, n)                                                                     \
    do {                                                                                           \
        setup;                                                                                     \
        resume_ = (n);                                                                             \
        return;                                                                                    \
    case (n):;                                                                                     \
    } while (false)
#define Z80_CYCLE(setup) Z80_CYCLE_AT(setup, __COUNTER__ + 1)
// Newer clangs (Emscripten's) call __COUNTER__ a C2y extension under -Wpedantic.
#if defined(__clang__)
#if __has_warning("-Wcounter-extension")
#pragma clang diagnostic ignored "-Wcounter-extension"
#endif
#endif

void Z80Core::start(const Z80Registers& r) {
    af_ = Word(r.a, r.f);
    bc_ = Word(r.b, r.c);
    de_ = Word(r.d, r.e);
    hl_ = Word(r.h, r.l);
    ix_ = r.ix;
    iy_ = r.iy;
    sp_ = r.sp;
    pc_ = r.pc;
    wz_ = r.wz;
    af2_ = r.af2;
    bc2_ = r.bc2;
    de2_ = r.de2;
    hl2_ = r.hl2;
    i_ = r.i;
    r_ = r.r;
    im_ = r.im;
    q_ = r.q;
    iff1_ = r.iff1;
    iff2_ = r.iff2;
    ei_ = r.ei;
    p_ = r.p;
    halted_ = false;
    nmiLatched_ = false;
    im0_ = false;
    resume_ = kStart;
    run();
}

void Z80Core::reset() {
    pc_ = 0;
    i_ = 0;
    r_ = 0;
    iff1_ = iff2_ = false;
    im_ = 0;
    af_ = 0xFFFF;
    sp_ = 0xFFFF;
    q_ = 0;
    ei_ = p_ = false;
    halted_ = false;
    nmiLatched_ = false;
    im0_ = false;
    resume_ = kStart;
    run();
}

Z80Registers Z80Core::registers() const {
    Z80Registers r;
    r.pc = pc_;
    r.sp = sp_;
    r.ix = ix_;
    r.iy = iy_;
    r.wz = wz_;
    r.a = a();
    r.f = f();
    r.b = Hi(bc_);
    r.c = Lo(bc_);
    r.d = Hi(de_);
    r.e = Lo(de_);
    r.h = Hi(hl_);
    r.l = Lo(hl_);
    r.i = i_;
    r.r = r_;
    r.af2 = af2_;
    r.bc2 = bc2_;
    r.de2 = de2_;
    r.hl2 = hl2_;
    r.im = im_;
    r.q = q_;
    r.iff1 = iff1_;
    r.iff2 = iff2_;
    r.ei = ei_;
    r.p = p_;
    return r;
}

void Z80Core::setNmi(bool asserted) {
    if (asserted && !nmiLevel_) {
        nmiLatched_ = true;
    }
    nmiLevel_ = asserted;
}

void Z80Core::step(std::uint8_t data) {
    data_ = data;
    run();
}

// --- Cycles -----------------------------------------------------------------------------------

void Z80Core::fetch() { cycle_ = Z80Cycle{Z80CycleKind::Fetch, pc_, 0, Word(i_, r_), 0, false}; }

// While halted the address bus shows the byte after the HALT, PC, fetched again and again and
// ignored. The ZX80 relies on it: its NOP gate and its glyph load (through IC5's latched character
// fetch) see HALT_N low, so that byte, the next row's first, is neither forced nor drawn.
void Z80Core::haltedFetch() {
    cycle_ = Z80Cycle{Z80CycleKind::Fetch, pc_, 0, Word(i_, r_), 0, true};
}

void Z80Core::read(std::uint16_t address) { cycle_ = Z80Cycle{Z80CycleKind::Read, address}; }

void Z80Core::write(std::uint16_t address, std::uint8_t data) {
    cycle_ = Z80Cycle{Z80CycleKind::Write, address, data};
}

void Z80Core::in(std::uint16_t port) { cycle_ = Z80Cycle{Z80CycleKind::In, port}; }

void Z80Core::out(std::uint16_t port, std::uint8_t data) {
    cycle_ = Z80Cycle{Z80CycleKind::Out, port, data};
}

void Z80Core::internal(int length) {
    cycle_.kind = Z80CycleKind::Internal;  // the address stays
    cycle_.length = static_cast<std::uint8_t>(length);
}

void Z80Core::acknowledge() {
    cycle_ = Z80Cycle{Z80CycleKind::Acknowledge, pc_, 0, Word(i_, r_), 0, false};
}

std::uint16_t Z80Core::pcNext() {
    const std::uint16_t at = pc_;
    if (!im0_) {
        ++pc_;
    }
    return at;
}

// --- Registers --------------------------------------------------------------------------------

void Z80Core::setF(std::uint8_t value) {
    af_ = Word(a(), value);
    flagsSet_ = true;
}

std::uint16_t& Z80Core::hl() { return index_ == 0 ? hl_ : index_ == 1 ? ix_ : iy_; }

std::uint16_t& Z80Core::rp(int p) {
    switch (p) {
    case 0:
        return bc_;
    case 1:
        return de_;
    case 2:
        return hl();
    default:
        return sp_;
    }
}

std::uint16_t& Z80Core::rp2(int p) { return p == 3 ? af_ : rp(p); }

std::uint8_t Z80Core::plainReg(int r) const {
    switch (r) {
    case 0:
        return Hi(bc_);
    case 1:
        return Lo(bc_);
    case 2:
        return Hi(de_);
    case 3:
        return Lo(de_);
    case 4:
        return Hi(hl_);
    case 5:
        return Lo(hl_);
    default:
        return a();
    }
}

void Z80Core::setPlainReg(int r, std::uint8_t v) {
    switch (r) {
    case 0:
        bc_ = Word(v, bc_);
        break;
    case 1:
        bc_ = Word(bc_ >> 8, v);
        break;
    case 2:
        de_ = Word(v, de_);
        break;
    case 3:
        de_ = Word(de_ >> 8, v);
        break;
    case 4:
        hl_ = Word(v, hl_);
        break;
    case 5:
        hl_ = Word(hl_ >> 8, v);
        break;
    default:
        setA(v);
        break;
    }
}

std::uint8_t Z80Core::reg(int r) const {
    if (index_ != 0 && (r == 4 || r == 5)) {
        const std::uint16_t xy = index_ == 1 ? ix_ : iy_;
        return r == 4 ? Hi(xy) : Lo(xy);
    }
    return plainReg(r);
}

void Z80Core::setReg(int r, std::uint8_t v) {
    if (index_ != 0 && (r == 4 || r == 5)) {
        std::uint16_t& xy = index_ == 1 ? ix_ : iy_;
        xy = r == 4 ? Word(v, xy) : Word(xy >> 8, v);
        return;
    }
    setPlainReg(r, v);
}

bool Z80Core::condition(int y) const {
    switch (y) {
    case 0:
        return (f() & kZ) == 0;
    case 1:
        return (f() & kZ) != 0;
    case 2:
        return (f() & kC) == 0;
    case 3:
        return (f() & kC) != 0;
    case 4:
        return (f() & kPv) == 0;
    case 5:
        return (f() & kPv) != 0;
    case 6:
        return (f() & kS) == 0;
    default:
        return (f() & kS) != 0;
    }
}

// --- ALU --------------------------------------------------------------------------------------

void Z80Core::alu(int op, std::uint8_t v) {
    const unsigned acc = a();
    const unsigned carry = f() & kC;
    unsigned result = 0;
    std::uint8_t flags = 0;
    switch (op) {
    case 0:  // ADD
    case 1:  // ADC
        result = acc + v + (op == 1 ? carry : 0);
        flags = static_cast<std::uint8_t>(Szxy(Lo(result)) | ((acc ^ v ^ result) & kH) |
                                          ((((acc ^ ~v) & (acc ^ result)) & 0x80) >> 5) |
                                          ((result >> 8) & kC));
        setA(Lo(result));
        break;
    case 2:  // SUB
    case 3:  // SBC
    case 7:  // CP
        result = acc - v - (op == 3 ? carry : 0);
        flags = static_cast<std::uint8_t>(Szxy(Lo(result)) | ((acc ^ v ^ result) & kH) |
                                          ((((acc ^ v) & (acc ^ result)) & 0x80) >> 5) | kN |
                                          ((result >> 8) & kC));
        if (op == 7) {
            flags = static_cast<std::uint8_t>((flags & ~kXy) | (v & kXy));  // X, Y: the operand
        } else {
            setA(Lo(result));
        }
        break;
    case 4:  // AND
        result = acc & v;
        flags = static_cast<std::uint8_t>(Szxy(Lo(result)) | kH | Parity(result));
        setA(Lo(result));
        break;
    case 5:  // XOR
        result = acc ^ v;
        flags = static_cast<std::uint8_t>(Szxy(Lo(result)) | Parity(result));
        setA(Lo(result));
        break;
    default:  // OR
        result = acc | v;
        flags = static_cast<std::uint8_t>(Szxy(Lo(result)) | Parity(result));
        setA(Lo(result));
        break;
    }
    setF(flags);
}

std::uint8_t Z80Core::inc(std::uint8_t v) {
    const auto result = static_cast<std::uint8_t>(v + 1);
    setF(static_cast<std::uint8_t>((f() & kC) | Szxy(result) | ((v & 0x0F) == 0x0F ? kH : 0) |
                                   (v == 0x7F ? kPv : 0)));
    return result;
}

std::uint8_t Z80Core::dec(std::uint8_t v) {
    const auto result = static_cast<std::uint8_t>(v - 1);
    setF(static_cast<std::uint8_t>((f() & kC) | kN | Szxy(result) | ((v & 0x0F) == 0 ? kH : 0) |
                                   (v == 0x80 ? kPv : 0)));
    return result;
}

std::uint8_t Z80Core::rotate(int op, std::uint8_t v) {
    unsigned result = 0, carry = 0;
    const unsigned c = f() & kC;
    switch (op) {
    case 0:  // RLC
        carry = v >> 7;
        result = (v << 1) | carry;
        break;
    case 1:  // RRC
        carry = v & 1;
        result = (v >> 1) | (carry << 7);
        break;
    case 2:  // RL
        carry = v >> 7;
        result = (v << 1) | c;
        break;
    case 3:  // RR
        carry = v & 1;
        result = (v >> 1) | (c << 7);
        break;
    case 4:  // SLA
        carry = v >> 7;
        result = v << 1;
        break;
    case 5:  // SRA
        carry = v & 1;
        result = (v >> 1) | (v & 0x80);
        break;
    case 6:  // SLL (undocumented): a 1 shifted in
        carry = v >> 7;
        result = (v << 1) | 1;
        break;
    default:  // SRL
        carry = v & 1;
        result = v >> 1;
        break;
    }
    const std::uint8_t r = Lo(result);
    setF(static_cast<std::uint8_t>(Szxy(r) | Parity(r) | carry));
    return r;
}

// BIT b: X and Y from `xy` (the register for BIT b,r; MEMPTR's high byte for (HL), the address's
// for (IX+d)).
void Z80Core::bit(int b, std::uint8_t v, std::uint8_t xy) {
    const unsigned set = v & (1u << b);
    setF(static_cast<std::uint8_t>((f() & kC) | kH | (xy & kXy) | (set == 0 ? kZ | kPv : 0) |
                                   (b == 7 && set != 0 ? kS : 0)));
}

void Z80Core::accumulatorOp(int y) {
    const unsigned acc = a();
    const std::uint8_t flags = f();
    const std::uint8_t kept = flags & (kS | kZ | kPv);
    unsigned result = acc, carry = 0;
    switch (y) {
    case 0:  // RLCA
        carry = acc >> 7;
        result = (acc << 1) | carry;
        break;
    case 1:  // RRCA
        carry = acc & 1;
        result = (acc >> 1) | (carry << 7);
        break;
    case 2:  // RLA
        carry = acc >> 7;
        result = (acc << 1) | (flags & kC);
        break;
    case 3:  // RRA
        carry = acc & 1;
        result = (acc >> 1) | ((flags & kC) << 7);
        break;
    case 4: {  // DAA
        const bool n = (flags & kN) != 0, h = (flags & kH) != 0, c = (flags & kC) != 0;
        unsigned diff = 0;
        if (h || (acc & 0x0F) > 9) {
            diff |= 0x06;
        }
        const bool newC = c || acc > 0x99;
        if (newC) {
            diff |= 0x60;
        }
        const bool newH = n ? h && (acc & 0x0F) < 6 : (acc & 0x0F) > 9;
        result = Lo(n ? acc - diff : acc + diff);
        setA(Lo(result));
        setF(static_cast<std::uint8_t>(Szxy(Lo(result)) | Parity(result) | (n ? kN : 0) |
                                       (newH ? kH : 0) | (newC ? kC : 0)));
        return;
    }
    case 5:  // CPL
        result = ~acc;
        setA(Lo(result));
        setF(static_cast<std::uint8_t>((flags & (kS | kZ | kPv | kC)) | kH | kN | (result & kXy)));
        return;
    case 6:  // SCF: X and Y from (Q ^ F) | A (Zilog NMOS, Rak 2012)
        setF(static_cast<std::uint8_t>(kept | kC | (((q_ ^ flags) | acc) & kXy)));
        return;
    default:  // CCF
        setF(static_cast<std::uint8_t>(kept | ((flags & kC) != 0 ? kH : kC) |
                                       (((q_ ^ flags) | acc) & kXy)));
        return;
    }
    setA(Lo(result));
    setF(static_cast<std::uint8_t>(kept | (result & kXy) | carry));
}

void Z80Core::add16(std::uint16_t v) {
    std::uint16_t& target = hl();
    const unsigned left = target;
    const unsigned result = left + v;
    wz_ = static_cast<std::uint16_t>(left + 1);
    setF(static_cast<std::uint8_t>((f() & (kS | kZ | kPv)) | (((left ^ v ^ result) >> 8) & kH) |
                                   ((result >> 16) & kC) | ((result >> 8) & kXy)));
    target = static_cast<std::uint16_t>(result);
}

void Z80Core::adc16(std::uint16_t v) {
    const unsigned left = hl_;
    const unsigned result = left + v + (f() & kC);
    wz_ = static_cast<std::uint16_t>(left + 1);
    hl_ = static_cast<std::uint16_t>(result);
    setF(static_cast<std::uint8_t>(
        ((result >> 8) & (kS | kXy)) | (hl_ == 0 ? kZ : 0) | (((left ^ v ^ result) >> 8) & kH) |
        (((~(left ^ v) & (left ^ result)) & 0x8000) >> 13) | ((result >> 16) & kC)));
}

void Z80Core::sbc16(std::uint16_t v) {
    const unsigned left = hl_;
    const unsigned result = left - v - (f() & kC);
    wz_ = static_cast<std::uint16_t>(left + 1);
    hl_ = static_cast<std::uint16_t>(result);
    setF(static_cast<std::uint8_t>(
        ((result >> 8) & (kS | kXy)) | (hl_ == 0 ? kZ : 0) | (((left ^ v ^ result) >> 8) & kH) |
        ((((left ^ v) & (left ^ result)) & 0x8000) >> 13) | kN | ((result >> 16) & kC)));
}

// LDI and family: X from bit 3 of A + value, Y from its bit 1.
void Z80Core::blockLoadFlags(std::uint8_t v) {
    const unsigned n = a() + v;
    setF(static_cast<std::uint8_t>((f() & (kS | kZ | kC)) | (bc_ != 0 ? kPv : 0) | (n & kX) |
                                   ((n << 4) & kY)));
}

// CPI and family: as CP but C kept; X and Y from A - value - H.
void Z80Core::blockCompareFlags(std::uint8_t v) {
    const unsigned result = Lo(a() - v);
    const unsigned h = (a() ^ v ^ result) & kH;
    const unsigned n = result - (h != 0 ? 1 : 0);
    setF(static_cast<std::uint8_t>((f() & kC) | kN | (result & kS) | (result == 0 ? kZ : 0) | h |
                                   (bc_ != 0 ? kPv : 0) | (n & kX) | ((n << 4) & kY)));
}

// INI, OUTI and family (Young, §4.3): k is the value plus C ± 1 (in) or L (out).
void Z80Core::blockIoFlags(std::uint8_t v, unsigned k) {
    const std::uint8_t b = Hi(bc_);
    setF(static_cast<std::uint8_t>(Szxy(b) | ((v & 0x80) != 0 ? kN : 0) | (k > 0xFF ? kH | kC : 0) |
                                   Parity((k & 7) ^ b)));
}

// A repeating block instruction (PC already back at it): X and Y from PC bits 11 and 13
// (Banks and Helcmanovsky, 2018).
void Z80Core::blockRepeatFlags() {
    setF(static_cast<std::uint8_t>((f() & ~kXy) | (Hi(pc_) & kXy)));
}

void Z80Core::blockIoRepeatFlags(std::uint8_t v, unsigned k) {
    blockRepeatFlags();
    const std::uint8_t b = Hi(bc_);
    std::uint8_t flags = f();
    if (k > 0xFF) {
        flags = static_cast<std::uint8_t>(flags & ~kH);
        if ((v & 0x80) != 0) {
            flags ^= static_cast<std::uint8_t>(Parity((b - 1) & 7) ^ kPv);
            flags |= (b & 0x0F) == 0x00 ? kH : 0;
        } else {
            flags ^= static_cast<std::uint8_t>(Parity((b + 1) & 7) ^ kPv);
            flags |= (b & 0x0F) == 0x0F ? kH : 0;
        }
    } else {
        flags ^= static_cast<std::uint8_t>(Parity(b & 7) ^ kPv);
    }
    setF(flags);
}

// --- The instruction sequencer ----------------------------------------------------------------

void Z80Core::run() {
    ended_ = false;
    switch (resume_) {
    default:
        return;

    case kStart:
        goto next;

    // An instruction or an interrupt sequence ended with this step.
    boundary:
        ended_ = true;
        ei_ = eiNow_;
        p_ = pNow_;
        q_ = flagsSet_ ? f() : 0;
        im0_ = false;
        if (nmiLatched_) {
            if (p_) {
                af_ = Word(a(), f() & ~kPv);  // NMOS: LD A,I / LD A,R interrupted
            }
            goto nmi;
        }
        if (int_ && iff1_ && !ei_) {
            if (p_) {
                af_ = Word(a(), f() & ~kPv);
            }
            goto irq;
        }
        if (halted_) {
            flagsSet_ = eiNow_ = pNow_ = false;
            Z80_CYCLE(haltedFetch());
            r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
            goto boundary;
        }

    next:
        index_ = 0;
        flagsSet_ = eiNow_ = pNow_ = false;
        Z80_CYCLE(fetch());
        pc_ = static_cast<std::uint16_t>(pc_ + (im0_ ? 0 : 1));
        r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
        op_ = data_;
        goto decode;

    prefix:  // DD or FD seen: the opcode follows
        Z80_CYCLE(fetch());
        pc_ = static_cast<std::uint16_t>(pc_ + (im0_ ? 0 : 1));
        r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
        op_ = data_;
        goto decode;

    decode:
        if (op_ == 0xDD || op_ == 0xFD) {
            index_ = op_ == 0xDD ? 1 : 2;
            q_ = 0;  // the prefix sets no flags: SCF and CCF after it see Q = 0 (Harte)
            goto prefix;
        }
        if (op_ == 0xCB) {
            if (index_ != 0) {
                goto indexedCb;
            }
            goto prefixCb;
        }
        if (op_ == 0xED) {
            goto prefixEd;
        }
        if (X(op_) == 1) {
            if (op_ == 0x76) {
                halted_ = true;  // PC is past the HALT; halted fetches at it follow
                goto boundary;
            }
            if (Y(op_) == 6) {
                goto ldHlR;
            }
            if (Z(op_) == 6) {
                goto ldRHl;
            }
            setReg(Y(op_), reg(Z(op_)));
            goto boundary;
        }
        if (X(op_) == 2) {
            if (Z(op_) == 6) {
                goto aluHl;
            }
            alu(Y(op_), reg(Z(op_)));
            goto boundary;
        }
        if (X(op_) == 0) {
            switch (Z(op_)) {
            case 0:
                if (Y(op_) == 0) {
                    goto boundary;  // NOP
                }
                if (Y(op_) == 1) {
                    std::swap(af_, af2_);
                    goto boundary;
                }
                if (Y(op_) == 2) {
                    goto djnz;
                }
                goto jr;
            case 1:
                if (Q(op_) == 0) {
                    goto ldRpNn;
                }
                goto addHlRp;
            case 2:
                goto indirectLoad;
            case 3:
                goto incDecRp;
            case 4:
            case 5:
                if (Y(op_) == 6) {
                    goto incDecHl;
                }
                setReg(Y(op_), Z(op_) == 4 ? inc(reg(Y(op_))) : dec(reg(Y(op_))));
                goto boundary;
            case 6:
                if (Y(op_) == 6) {
                    goto ldHlN;
                }
                goto ldRN;
            default:
                accumulatorOp(Y(op_));
                goto boundary;
            }
        }
        // X == 3
        switch (Z(op_)) {
        case 0:
            goto retCc;
        case 1:
            if (Q(op_) == 0) {
                goto pop;
            }
            switch (P(op_)) {
            case 0:
                goto ret;
            case 1:
                std::swap(bc_, bc2_);
                std::swap(de_, de2_);
                std::swap(hl_, hl2_);
                goto boundary;
            case 2:
                pc_ = hl();  // JP (HL)
                goto boundary;
            default:
                goto ldSpHl;
            }
        case 2:
            goto jpCcNn;
        case 3:
            switch (Y(op_)) {
            case 0:
                goto jpNn;
            case 2:
                goto outNA;
            case 3:
                goto inAN;
            case 4:
                goto exSpHl;
            case 5:
                std::swap(de_, hl_);  // EX DE,HL: never IX
                goto boundary;
            case 6:
                iff1_ = iff2_ = false;  // DI
                goto boundary;
            default:
                iff1_ = iff2_ = true;  // EI
                eiNow_ = true;
                goto boundary;
            }
        case 4:
            goto callCcNn;
        case 5:
            if (Q(op_) == 0) {
                goto push;
            }
            goto callNn;
        case 6:
            goto aluN;
        default:
            goto rst;
        }

        // --- Unprefixed and DD/FD
        // -----------------------------------------------------------------

    ldRN:  // LD r,n
        Z80_CYCLE(read(pcNext()));
        setReg(Y(op_), data_);
        goto boundary;

    ldHlN:  // LD (HL),n; LD (IX+d),n: d, then n with 2 internal T-states
        if (index_ != 0) {
            Z80_CYCLE(read(pcNext()));
            address_ = static_cast<std::uint16_t>(hl() + static_cast<std::int8_t>(data_));
            wz_ = address_;
            Z80_CYCLE(read(pcNext()));
            value_ = data_;
            Z80_CYCLE(internal(2));
        } else {
            address_ = hl_;
            Z80_CYCLE(read(pcNext()));
            value_ = data_;
        }
        Z80_CYCLE(write(address_, value_));
        goto boundary;

    ldRHl:  // LD r,(HL); LD r,(IX+d) (r never IXH or IXL)
        if (index_ != 0) {
            Z80_CYCLE(read(pcNext()));
            address_ = static_cast<std::uint16_t>(hl() + static_cast<std::int8_t>(data_));
            wz_ = address_;
            Z80_CYCLE(internal(5));
        } else {
            address_ = hl_;
        }
        Z80_CYCLE(read(address_));
        setPlainReg(Y(op_), data_);
        goto boundary;

    ldHlR:  // LD (HL),r; LD (IX+d),r
        if (index_ != 0) {
            Z80_CYCLE(read(pcNext()));
            address_ = static_cast<std::uint16_t>(hl() + static_cast<std::int8_t>(data_));
            wz_ = address_;
            Z80_CYCLE(internal(5));
        } else {
            address_ = hl_;
        }
        Z80_CYCLE(write(address_, plainReg(Z(op_))));
        goto boundary;

    aluHl:  // ALU A,(HL); ALU A,(IX+d)
        if (index_ != 0) {
            Z80_CYCLE(read(pcNext()));
            address_ = static_cast<std::uint16_t>(hl() + static_cast<std::int8_t>(data_));
            wz_ = address_;
            Z80_CYCLE(internal(5));
        } else {
            address_ = hl_;
        }
        Z80_CYCLE(read(address_));
        alu(Y(op_), data_);
        goto boundary;

    aluN:
        Z80_CYCLE(read(pcNext()));
        alu(Y(op_), data_);
        goto boundary;

    incDecHl:  // INC (HL), DEC (HL); (IX+d)
        if (index_ != 0) {
            Z80_CYCLE(read(pcNext()));
            address_ = static_cast<std::uint16_t>(hl() + static_cast<std::int8_t>(data_));
            wz_ = address_;
            Z80_CYCLE(internal(5));
        } else {
            address_ = hl_;
        }
        Z80_CYCLE(read(address_));
        value_ = Z(op_) == 4 ? inc(data_) : dec(data_);
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(address_, value_));
        goto boundary;

    djnz:
        Z80_CYCLE(internal(1));
        Z80_CYCLE(read(pcNext()));
        bc_ = Word(Hi(bc_) - 1, bc_);
        if (Hi(bc_) != 0) {
            value_ = data_;
            Z80_CYCLE(internal(5));
            pc_ = static_cast<std::uint16_t>(pc_ + static_cast<std::int8_t>(value_));
            wz_ = pc_;
        }
        goto boundary;

    jr:  // JR e, JR cc,e
        Z80_CYCLE(read(pcNext()));
        if (Y(op_) == 3 || condition(Y(op_) - 4)) {
            value_ = data_;
            Z80_CYCLE(internal(5));
            pc_ = static_cast<std::uint16_t>(pc_ + static_cast<std::int8_t>(value_));
            wz_ = pc_;
        }
        goto boundary;

    ldRpNn:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        rp(P(op_)) = Word(data_, value_);
        goto boundary;

    addHlRp:
        Z80_CYCLE(internal(7));
        add16(rp(P(op_)));
        goto boundary;

    indirectLoad:  // x = 0, z = 2
        switch (P(op_)) {
        case 0:
        case 1:
            address_ = P(op_) == 0 ? bc_ : de_;
            if (Q(op_) == 0) {
                goto ldRpA;
            }
            goto ldARp;
        case 2:
            if (Q(op_) == 0) {
                goto ldNnHl;
            }
            goto ldHlNn;
        default:
            if (Q(op_) == 0) {
                goto ldNnA;
            }
            goto ldANn;
        }

    ldRpA:  // LD (BC),A; LD (DE),A
        Z80_CYCLE(write(address_, a()));
        wz_ = Word(a(), address_ + 1);
        goto boundary;

    ldARp:
        Z80_CYCLE(read(address_));
        setA(data_);
        wz_ = static_cast<std::uint16_t>(address_ + 1);
        goto boundary;

    ldNnHl:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        address_ = Word(data_, value_);
        Z80_CYCLE(write(address_, Lo(hl())));
        wz_ = static_cast<std::uint16_t>(address_ + 1);
        Z80_CYCLE(write(wz_, Hi(hl())));
        goto boundary;

    ldHlNn:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        address_ = Word(data_, value_);
        Z80_CYCLE(read(address_));
        value_ = data_;
        wz_ = static_cast<std::uint16_t>(address_ + 1);
        Z80_CYCLE(read(wz_));
        hl() = Word(data_, value_);
        goto boundary;

    ldNnA:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        address_ = Word(data_, value_);
        Z80_CYCLE(write(address_, a()));
        wz_ = Word(a(), address_ + 1);
        goto boundary;

    ldANn:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        address_ = Word(data_, value_);
        Z80_CYCLE(read(address_));
        setA(data_);
        wz_ = static_cast<std::uint16_t>(address_ + 1);
        goto boundary;

    incDecRp:
        Z80_CYCLE(internal(2));
        rp(P(op_)) = static_cast<std::uint16_t>(rp(P(op_)) + (Q(op_) == 0 ? 1 : -1));
        goto boundary;

    retCc:
        Z80_CYCLE(internal(1));
        if (!condition(Y(op_))) {
            goto boundary;
        }
        goto ret;

    ret:
        Z80_CYCLE(read(sp_++));
        value_ = data_;
        Z80_CYCLE(read(sp_++));
        pc_ = Word(data_, value_);
        wz_ = pc_;
        goto boundary;

    pop:
        Z80_CYCLE(read(sp_++));
        value_ = data_;
        Z80_CYCLE(read(sp_++));
        rp2(P(op_)) = Word(data_, value_);
        goto boundary;

    push:
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(--sp_, Hi(rp2(P(op_)))));
        Z80_CYCLE(write(--sp_, Lo(rp2(P(op_)))));
        goto boundary;

    ldSpHl:
        Z80_CYCLE(internal(2));
        sp_ = hl();
        goto boundary;

    jpNn:
    jpCcNn:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        wz_ = Word(data_, value_);
        if (op_ == 0xC3 || condition(Y(op_))) {
            pc_ = wz_;
        }
        goto boundary;

    callNn:
    callCcNn:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        wz_ = Word(data_, value_);
        if (op_ != 0xCD && !condition(Y(op_))) {
            goto boundary;
        }
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(--sp_, Hi(pc_)));
        Z80_CYCLE(write(--sp_, Lo(pc_)));
        pc_ = wz_;
        goto boundary;

    rst:
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(--sp_, Hi(pc_)));
        Z80_CYCLE(write(--sp_, Lo(pc_)));
        pc_ = static_cast<std::uint16_t>(Y(op_) * 8);
        wz_ = pc_;
        goto boundary;

    outNA:
        Z80_CYCLE(read(pcNext()));
        address_ = Word(a(), data_);
        Z80_CYCLE(out(address_, a()));
        wz_ = Word(a(), address_ + 1);
        goto boundary;

    inAN:
        Z80_CYCLE(read(pcNext()));
        address_ = Word(a(), data_);
        Z80_CYCLE(in(address_));
        setA(data_);
        wz_ = static_cast<std::uint16_t>(address_ + 1);
        goto boundary;

    exSpHl:
        Z80_CYCLE(read(sp_));
        value_ = data_;
        Z80_CYCLE(read(static_cast<std::uint16_t>(sp_ + 1)));
        address_ = Word(data_, value_);  // the word from the stack
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(static_cast<std::uint16_t>(sp_ + 1), Hi(hl())));
        Z80_CYCLE(write(sp_, Lo(hl())));
        Z80_CYCLE(internal(2));
        hl() = address_;
        wz_ = address_;
        goto boundary;

        // --- CB
        // -----------------------------------------------------------------------------------

    prefixCb:
        Z80_CYCLE(fetch());
        pc_ = static_cast<std::uint16_t>(pc_ + (im0_ ? 0 : 1));
        r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
        op_ = data_;
        if (Z(op_) != 6) {
            value_ = plainReg(Z(op_));
            if (X(op_) == 0) {
                setPlainReg(Z(op_), rotate(Y(op_), value_));
            } else if (X(op_) == 1) {
                bit(Y(op_), value_, value_);
            } else if (X(op_) == 2) {
                setPlainReg(Z(op_), static_cast<std::uint8_t>(value_ & ~(1u << Y(op_))));
            } else {
                setPlainReg(Z(op_), static_cast<std::uint8_t>(value_ | (1u << Y(op_))));
            }
            goto boundary;
        }
        address_ = hl_;
        goto cbMemory;

    indexedCb:  // DD CB d op: d, then op (a read, not an M1), then the operand
        Z80_CYCLE(read(pcNext()));
        address_ = static_cast<std::uint16_t>(hl() + static_cast<std::int8_t>(data_));
        wz_ = address_;
        Z80_CYCLE(read(pcNext()));
        op_ = data_;
        Z80_CYCLE(internal(2));
        goto cbMemory;

    cbMemory:
        Z80_CYCLE(read(address_));
        value_ = data_;
        Z80_CYCLE(internal(1));
        if (X(op_) == 1) {
            bit(Y(op_), value_, Hi(wz_));
            goto boundary;
        }
        if (X(op_) == 0) {
            value_ = rotate(Y(op_), value_);
        } else if (X(op_) == 2) {
            value_ = static_cast<std::uint8_t>(value_ & ~(1u << Y(op_)));
        } else {
            value_ = static_cast<std::uint8_t>(value_ | (1u << Y(op_)));
        }
        if (index_ != 0 && Z(op_) != 6) {
            setPlainReg(Z(op_), value_);  // undocumented: the result also to a register
        }
        Z80_CYCLE(write(address_, value_));
        goto boundary;

        // --- ED
        // -----------------------------------------------------------------------------------

    prefixEd:
        index_ = 0;  // DD ED: the DD is ignored
        Z80_CYCLE(fetch());
        pc_ = static_cast<std::uint16_t>(pc_ + (im0_ ? 0 : 1));
        r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
        op_ = data_;
        if (X(op_) == 2 && Z(op_) <= 3 && Y(op_) >= 4) {
            switch (Z(op_)) {
            case 0:
                goto ldi;
            case 1:
                goto cpi;
            case 2:
                goto ini;
            default:
                goto outi;
            }
        }
        if (X(op_) != 1) {
            goto boundary;  // the holes: 8 T-states, nothing done
        }
        switch (Z(op_)) {
        case 0:
            goto inRC;
        case 1:
            goto outCR;
        case 2:
            goto adcSbcHl;
        case 3:
            goto ldNnRpEd;
        case 4: {
            const std::uint8_t v = a();  // NEG
            setA(0);
            alu(2, v);
            goto boundary;
        }
        case 5:
            goto retn;
        case 6:
            im_ = static_cast<std::uint8_t>(std::array<int, 8>{0, 0, 1, 2, 0, 0, 1, 2}[Y(op_)]);
            goto boundary;
        default:
            switch (Y(op_)) {
            case 0:
            case 1:
            case 2:
            case 3:
                goto ldIr;
            case 4:
            case 5:
                goto rrdRld;
            default:
                goto boundary;  // ED 77, ED 7F: NOPs
            }
        }

    inRC:  // IN r,(C); IN (C) (y = 6: flags only)
        Z80_CYCLE(in(bc_));
        wz_ = static_cast<std::uint16_t>(bc_ + 1);
        if (Y(op_) != 6) {
            setPlainReg(Y(op_), data_);
        }
        setF(static_cast<std::uint8_t>((f() & kC) | Szxy(data_) | Parity(data_)));
        goto boundary;

    outCR:  // OUT (C),r; OUT (C),0 (y = 6, NMOS)
        Z80_CYCLE(out(bc_, Y(op_) == 6 ? 0 : plainReg(Y(op_))));
        wz_ = static_cast<std::uint16_t>(bc_ + 1);
        goto boundary;

    adcSbcHl:
        Z80_CYCLE(internal(7));
        if (Q(op_) == 0) {
            sbc16(rp(P(op_)));
        } else {
            adc16(rp(P(op_)));
        }
        goto boundary;

    ldNnRpEd:
        Z80_CYCLE(read(pcNext()));
        value_ = data_;
        Z80_CYCLE(read(pcNext()));
        address_ = Word(data_, value_);
        wz_ = static_cast<std::uint16_t>(address_ + 1);
        if (Q(op_) == 0) {
            Z80_CYCLE(write(address_, Lo(rp(P(op_)))));
            Z80_CYCLE(write(wz_, Hi(rp(P(op_)))));
        } else {
            Z80_CYCLE(read(address_));
            value_ = data_;
            Z80_CYCLE(read(wz_));
            rp(P(op_)) = Word(data_, value_);
        }
        goto boundary;

    retn:  // RETN, RETI and their mirrors: IFF1 = IFF2
        Z80_CYCLE(read(sp_++));
        value_ = data_;
        Z80_CYCLE(read(sp_++));
        pc_ = Word(data_, value_);
        wz_ = pc_;
        iff1_ = iff2_;
        goto boundary;

    ldIr:  // LD I,A; LD R,A; LD A,I; LD A,R
        Z80_CYCLE(internal(1));
        switch (Y(op_)) {
        case 0:
            i_ = a();
            break;
        case 1:
            r_ = a();
            break;
        default: {
            const std::uint8_t v = Y(op_) == 2 ? i_ : r_;
            setA(v);
            setF(static_cast<std::uint8_t>((f() & kC) | Szxy(v) | (iff2_ ? kPv : 0)));
            pNow_ = true;
            break;
        }
        }
        goto boundary;

    rrdRld:
        Z80_CYCLE(read(hl_));
        value_ = data_;
        Z80_CYCLE(internal(4));
        if (Y(op_) == 4) {  // RRD
            address_ = Word(0, a() << 4 | value_ >> 4);
            setA(static_cast<std::uint8_t>((a() & 0xF0) | (value_ & 0x0F)));
        } else {  // RLD
            address_ = Word(0, value_ << 4 | (a() & 0x0F));
            setA(static_cast<std::uint8_t>((a() & 0xF0) | (value_ >> 4)));
        }
        setF(static_cast<std::uint8_t>((f() & kC) | Szxy(a()) | Parity(a())));
        wz_ = static_cast<std::uint16_t>(hl_ + 1);
        Z80_CYCLE(write(hl_, Lo(address_)));
        goto boundary;

    // Block instructions: y = 4 increment, 5 decrement, 6 and 7 the same repeating.
    ldi:
        Z80_CYCLE(read(hl_));
        value_ = data_;
        Z80_CYCLE(write(de_, value_));
        Z80_CYCLE(internal(2));
        hl_ = static_cast<std::uint16_t>(hl_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        de_ = static_cast<std::uint16_t>(de_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        --bc_;
        blockLoadFlags(value_);
        if (Y(op_) >= 6 && bc_ != 0) {
            Z80_CYCLE(internal(5));
            pc_ = static_cast<std::uint16_t>(pc_ - 2);
            wz_ = static_cast<std::uint16_t>(pc_ + 1);
            blockRepeatFlags();
        }
        goto boundary;

    cpi:
        Z80_CYCLE(read(hl_));
        value_ = data_;
        Z80_CYCLE(internal(5));
        hl_ = static_cast<std::uint16_t>(hl_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        wz_ = static_cast<std::uint16_t>(wz_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        --bc_;
        blockCompareFlags(value_);
        if (Y(op_) >= 6 && bc_ != 0 && (f() & kZ) == 0) {
            Z80_CYCLE(internal(5));
            pc_ = static_cast<std::uint16_t>(pc_ - 2);
            wz_ = static_cast<std::uint16_t>(pc_ + 1);
            blockRepeatFlags();
        }
        goto boundary;

    ini:
        Z80_CYCLE(internal(1));
        Z80_CYCLE(in(bc_));
        value_ = data_;
        wz_ = static_cast<std::uint16_t>(bc_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        bc_ = Word(Hi(bc_) - 1, bc_);
        Z80_CYCLE(write(hl_, value_));
        hl_ = static_cast<std::uint16_t>(hl_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        address_ = static_cast<std::uint16_t>(value_ + Lo(Lo(bc_) + ((Y(op_) & 1) == 0 ? 1 : -1)));
        blockIoFlags(value_, address_);
        if (Y(op_) >= 6 && Hi(bc_) != 0) {
            Z80_CYCLE(internal(5));
            pc_ = static_cast<std::uint16_t>(pc_ - 2);
            wz_ = static_cast<std::uint16_t>(pc_ + 1);
            blockIoRepeatFlags(value_, address_);
        }
        goto boundary;

    outi:
        Z80_CYCLE(internal(1));
        Z80_CYCLE(read(hl_));
        value_ = data_;
        bc_ = Word(Hi(bc_) - 1, bc_);
        wz_ = static_cast<std::uint16_t>(bc_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        Z80_CYCLE(out(bc_, value_));
        hl_ = static_cast<std::uint16_t>(hl_ + ((Y(op_) & 1) == 0 ? 1 : -1));
        address_ = static_cast<std::uint16_t>(value_ + Lo(hl_));
        blockIoFlags(value_, address_);
        if (Y(op_) >= 6 && Hi(bc_) != 0) {
            Z80_CYCLE(internal(5));
            pc_ = static_cast<std::uint16_t>(pc_ - 2);
            wz_ = static_cast<std::uint16_t>(pc_ + 1);
            blockIoRepeatFlags(value_, address_);
        }
        goto boundary;

        // --- Interrupts
        // ---------------------------------------------------------------------------

    nmi:  // a fetch at PC (opcode ignored), the push, PC = $0066
        nmiLatched_ = false;
        halted_ = false;
        iff1_ = false;
        flagsSet_ = eiNow_ = pNow_ = false;
        index_ = 0;
        Z80_CYCLE(fetch());
        r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(--sp_, Hi(pc_)));
        Z80_CYCLE(write(--sp_, Lo(pc_)));
        pc_ = 0x0066;
        wz_ = pc_;
        goto boundary;

    irq:
        iff1_ = iff2_ = false;
        halted_ = false;
        flagsSet_ = eiNow_ = pNow_ = false;
        index_ = 0;
        Z80_CYCLE(acknowledge());
        r_ = static_cast<std::uint8_t>((r_ & 0x80) | ((r_ + 1) & 0x7F));
        if (im_ == 0) {
            im0_ = true;  // the byte read is the opcode; its operands are read at PC, not counted
            op_ = data_;
            goto decode;
        }
        value_ = data_;
        Z80_CYCLE(internal(1));
        Z80_CYCLE(write(--sp_, Hi(pc_)));
        Z80_CYCLE(write(--sp_, Lo(pc_)));
        if (im_ == 1) {
            pc_ = 0x0038;
        } else {
            address_ = Word(i_, value_);
            Z80_CYCLE(read(address_));
            value_ = data_;
            Z80_CYCLE(read(static_cast<std::uint16_t>(address_ + 1)));
            pc_ = Word(data_, value_);
        }
        wz_ = pc_;
        goto boundary;
    }
}

#undef Z80_CYCLE
#undef Z80_CYCLE_AT

}  // namespace bw
