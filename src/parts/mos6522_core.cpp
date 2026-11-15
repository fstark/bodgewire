#include "parts/mos6522_core.hpp"

namespace bw {

namespace {

// IFR and IER bits (R6522 Fig. 29).
constexpr std::uint8_t kCa2 = 0x01, kCa1 = 0x02, kSr = 0x04, kCb2 = 0x08, kCb1 = 0x10, kT2 = 0x20,
                       kT1 = 0x40;

// C2 modes (PCR): 0-3 inputs (bit 1 rising, bit 0 independent), 4 handshake, 5 pulse, 6 low,
// 7 high.
constexpr bool Independent(unsigned mode) { return mode < 4 && (mode & 1u) != 0; }
constexpr bool Strobed(unsigned mode) { return mode == 4 || mode == 5; }
constexpr bool C2Level(unsigned mode, bool strobe) {
    return mode == 6 ? false : mode == 7 || strobe;
}

}  // namespace

void Mos6522Core::reset() {
    ora_ = orb_ = ddra_ = ddrb_ = acr_ = pcr_ = ifr_ = ier_ = 0;
    t1Armed_ = t2Armed_ = false;
    pb7_ = true;
    ca2Out_ = cb2Out_ = true;
    ca2StrobeFall_ = ca2StrobeRise_ = ca2EndFall_ = ca2EndRise_ = false;
    cb2StrobeRise_ = cb2EndRise_ = false;
    srRunning_ = srStarted_ = false;
    cb1Out_ = srCb2_ = true;
    srBits_ = 0;
}

void Mos6522Core::randomise(std::span<const std::uint8_t, 16> bytes) {
    reset();
    ora_ = bytes[0];
    orb_ = bytes[1];
    ddra_ = bytes[2];
    ddrb_ = bytes[3];
    acr_ = bytes[4];
    pcr_ = bytes[5];
    t1Counter_ = static_cast<std::uint16_t>(bytes[6] | bytes[7] << 8);
    t1Latch_ = static_cast<std::uint16_t>(bytes[8] | bytes[9] << 8);
    t2Counter_ = static_cast<std::uint16_t>(bytes[10] | bytes[11] << 8);
    t2LatchLow_ = bytes[12];
    sr_ = bytes[13];
    ifr_ = bytes[14] & 0x7Fu;
    ier_ = bytes[15] & 0x7Fu;
    sampled_ = false;
    t1Loaded_ = t1Reload_ = t2Loaded_ = false;
}

bool Mos6522Core::cb1IsOutput() const {
    const unsigned mode = srMode();
    return mode != 0 && mode != 3 && mode != 7;
}

void Mos6522Core::clearA() {
    ifr_ &= static_cast<std::uint8_t>(~kCa1);
    if (!Independent(ca2Mode())) {
        ifr_ &= static_cast<std::uint8_t>(~kCa2);
    }
}

void Mos6522Core::clearB() {
    ifr_ &= static_cast<std::uint8_t>(~kCb1);
    if (!Independent(cb2Mode())) {
        ifr_ &= static_cast<std::uint8_t>(~kCb2);
    }
}

void Mos6522Core::startShift() {
    ifr_ &= static_cast<std::uint8_t>(~kSr);
    srBits_ = 0;
    srRunning_ = srMode() != 0;
    srDivider_ = t2LatchLow_ + 2u;
    srStarted_ = true;  // the clock starts from the next cycle
    if (cb1IsOutput()) {
        cb1Out_ = true;
    }
}

// The shift register's clock moved: out modes put the next bit on CB2 as CB1 falls and count it
// as CB1 rises; in modes take CB2 in as CB1 rises. 8 bits set the flag and stop, but mode 7 goes
// on and mode 4 never flags.
void Mos6522Core::shiftEdge(bool cb1Rising) {
    const unsigned mode = srMode();
    if (!srRunning_ && mode != 4) {
        return;
    }
    const bool out = mode >= 4;
    if (out && !cb1Rising) {
        const bool bit = (sr_ & 0x80u) != 0;
        sr_ = static_cast<std::uint8_t>(sr_ << 1 | (bit ? 1u : 0u));
        srCb2_ = bit;
        return;
    }
    if (!cb1Rising) {
        return;
    }
    if (!out) {
        sr_ = static_cast<std::uint8_t>(sr_ << 1 | (in_.cb2 ? 1u : 0u));
    }
    if (mode == 4) {
        return;
    }
    if (++srBits_ == 8) {
        srBits_ = 0;
        ifr_ |= kSr;
        if (mode != 7) {
            srRunning_ = false;
        }
    }
}

void Mos6522Core::rise(const Inputs& in) {
    const Inputs old = in_;
    in_ = in;
    if (sampled_) {
        // CA1, CB1 active edges: the flag, the input latch, the end of a C2 handshake.
        if (in.ca1 != old.ca1 && in.ca1 == ((pcr_ & 0x01u) != 0)) {
            ifr_ |= kCa1;
            if ((acr_ & 0x01u) != 0) {
                ira_ = in.pa;
            }
            if (ca2Mode() == 4) {
                ca2Out_ = true;
            }
        }
        if (ca2Mode() < 4 && in.ca2 != old.ca2 && in.ca2 == ((ca2Mode() & 2u) != 0)) {
            ifr_ |= kCa2;
        }
        if (!cb1IsOutput() && in.cb1 != old.cb1) {
            if (in.cb1 == ((pcr_ & 0x10u) != 0)) {
                ifr_ |= kCb1;
                if ((acr_ & 0x02u) != 0) {
                    irb_ = in.pb;
                }
                if (cb2Mode() == 4) {
                    cb2Out_ = true;
                }
            }
            if (srMode() == 3 || srMode() == 7) {
                shiftEdge(in.cb1);  // external clock
            }
        }
        if (srMode() == 0 && cb2Mode() < 4 && in.cb2 != old.cb2 &&
            in.cb2 == ((cb2Mode() & 2u) != 0)) {
            ifr_ |= kCb2;
        }
        // T2 counting PB6 pulses (ACR bit 5): a fall since the last rise.
        if ((acr_ & 0x20u) != 0 && (old.pb & 0x40u) != 0 && (in.pb & 0x40u) == 0) {
            --t2Counter_;
            if (t2Counter_ == 0 && t2Armed_) {
                ifr_ |= kT2;
                t2Armed_ = false;
            }
        }
    }
    sampled_ = true;
    // Write strobes start at the rise after the write; their pulses end one rise later.
    if (ca2EndRise_) {
        ca2EndRise_ = false;
        ca2Out_ = true;
    }
    if (ca2StrobeRise_) {
        ca2StrobeRise_ = false;
        ca2Out_ = false;
        ca2EndRise_ = ca2Mode() == 5;
    }
    if (cb2EndRise_) {
        cb2EndRise_ = false;
        cb2Out_ = true;
    }
    if (cb2StrobeRise_) {
        cb2StrobeRise_ = false;
        cb2Out_ = false;
        cb2EndRise_ = cb2Mode() == 5;
    }
}

std::uint8_t Mos6522Core::read(unsigned reg) {
    switch (reg & 0xFu) {
    case 0x0: {
        const std::uint8_t pins = (acr_ & 0x02u) != 0 ? irb_ : in_.pb;
        clearB();
        return static_cast<std::uint8_t>((orb_ & ddrb_) | (pins & ~ddrb_));
    }
    case 0x1:
        clearA();
        if (Strobed(ca2Mode())) {
            ca2StrobeFall_ = true;
        }
        return (acr_ & 0x01u) != 0 ? ira_ : in_.pa;
    case 0x2:
        return ddrb_;
    case 0x3:
        return ddra_;
    case 0x4:
        ifr_ &= static_cast<std::uint8_t>(~kT1);
        return static_cast<std::uint8_t>(t1Counter_);
    case 0x5:
        return static_cast<std::uint8_t>(t1Counter_ >> 8);
    case 0x6:
        return static_cast<std::uint8_t>(t1Latch_);
    case 0x7:
        return static_cast<std::uint8_t>(t1Latch_ >> 8);
    case 0x8:
        ifr_ &= static_cast<std::uint8_t>(~kT2);
        return static_cast<std::uint8_t>(t2Counter_);
    case 0x9:
        return static_cast<std::uint8_t>(t2Counter_ >> 8);
    case 0xA: {
        const std::uint8_t value = sr_;
        startShift();
        return value;
    }
    case 0xB:
        return acr_;
    case 0xC:
        return pcr_;
    case 0xD:
        return static_cast<std::uint8_t>(ifr_ | ((ifr_ & ier_ & 0x7Fu) != 0 ? 0x80u : 0u));
    case 0xE:
        return static_cast<std::uint8_t>(ier_ | 0x80u);  // bit 7 reads 1 (R6522, MAME)
    default:                                             // 0xF: port A without handshake
        return (acr_ & 0x01u) != 0 ? ira_ : in_.pa;
    }
}

void Mos6522Core::write(unsigned reg, std::uint8_t v) {
    switch (reg & 0xFu) {
    case 0x0:
        orb_ = v;
        clearB();
        cb2StrobeRise_ = Strobed(cb2Mode()) && srMode() == 0;
        break;
    case 0x1:
        ora_ = v;
        clearA();
        ca2StrobeRise_ = Strobed(ca2Mode());
        break;
    case 0x2:
        ddrb_ = v;
        break;
    case 0x3:
        ddra_ = v;
        break;
    case 0x4:
    case 0x6:
        t1Latch_ = static_cast<std::uint16_t>((t1Latch_ & 0xFF00u) | v);
        break;
    case 0x5:
        t1Latch_ = static_cast<std::uint16_t>(v << 8 | (t1Latch_ & 0xFFu));
        t1Counter_ = t1Latch_;
        ifr_ &= static_cast<std::uint8_t>(~kT1);
        t1Loaded_ = true;
        t1Reload_ = false;
        t1Armed_ = true;
        pb7_ = false;
        break;
    case 0x7:
        t1Latch_ = static_cast<std::uint16_t>(v << 8 | (t1Latch_ & 0xFFu));
        ifr_ &= static_cast<std::uint8_t>(~kT1);
        break;
    case 0x8:
        t2LatchLow_ = v;
        break;
    case 0x9:
        t2Counter_ = static_cast<std::uint16_t>(v << 8 | t2LatchLow_);
        ifr_ &= static_cast<std::uint8_t>(~kT2);
        t2Loaded_ = true;
        t2Armed_ = true;
        break;
    case 0xA:
        sr_ = v;
        startShift();
        break;
    case 0xB:
        acr_ = v;
        if (srMode() == 0) {
            srRunning_ = false;
        }
        if (!cb1IsOutput()) {
            cb1Out_ = true;
        }
        break;
    case 0xC:
        pcr_ = v;
        if (Strobed(ca2Mode())) {
            ca2Out_ = true;
        }
        if (Strobed(cb2Mode())) {
            cb2Out_ = true;
        }
        break;
    case 0xD:
        ifr_ &= static_cast<std::uint8_t>(~v & 0x7Fu);
        break;
    case 0xE:
        if ((v & 0x80u) != 0) {
            ier_ |= v & 0x7Fu;
        } else {
            ier_ &= static_cast<std::uint8_t>(~v & 0x7Fu);
        }
        break;
    default:  // 0xF: port A without handshake
        ora_ = v;
        break;
    }
}

void Mos6522Core::fall() {
    // Read strobes start at the read's own fall; their pulses end one fall later.
    if (ca2EndFall_) {
        ca2EndFall_ = false;
        ca2Out_ = true;
    }
    if (ca2StrobeFall_) {
        ca2StrobeFall_ = false;
        ca2Out_ = false;
        ca2EndFall_ = ca2Mode() == 5;
    }
    // T1: N, N-1 ... 0, $FFFF (the flag), then N again, in both modes (MAME, VICE).
    if (t1Loaded_) {
        t1Loaded_ = false;
    } else if (t1Reload_) {
        t1Reload_ = false;
        t1Counter_ = t1Latch_;
    } else if (t1Counter_ == 0) {
        t1Counter_ = 0xFFFF;
        t1Reload_ = true;
        if (t1Armed_) {
            ifr_ |= kT1;
            if ((acr_ & 0x40u) != 0) {
                pb7_ = !pb7_;  // free-running: a square wave
            } else {
                pb7_ = true;  // one-shot: one pulse low, one flag
                t1Armed_ = false;
            }
        }
    } else {
        --t1Counter_;
    }
    // T2 as a one-shot timer: no reload, one flag a write.
    if ((acr_ & 0x20u) == 0) {
        if (t2Loaded_) {
            t2Loaded_ = false;
        } else {
            if (t2Counter_ == 0 && t2Armed_) {
                ifr_ |= kT2;
                t2Armed_ = false;
            }
            --t2Counter_;
        }
    }
    // The shift register's own clock: φ2 (modes 2, 6) or T2's low latch (1, 4, 5), CB1 a half
    // period of 1 or N + 2 cycles.
    const unsigned mode = srMode();
    if (srStarted_) {
        srStarted_ = false;
    } else if ((srRunning_ || mode == 4) && cb1IsOutput()) {
        bool toggle = mode == 2 || mode == 6;
        if (!toggle) {
            if (srDivider_ == 0) {
                srDivider_ = t2LatchLow_ + 2u;
            }
            if (--srDivider_ == 0) {
                toggle = true;
                srDivider_ = t2LatchLow_ + 2u;
            }
        }
        if (toggle) {
            cb1Out_ = !cb1Out_;
            shiftEdge(cb1Out_);
        }
    }
}

Mos6522Core::Outputs Mos6522Core::outputs() const {
    Outputs o;
    o.paDrive = ddra_;
    o.paLevel = ora_;
    o.pbDrive = ddrb_;
    o.pbLevel = orb_;
    if ((acr_ & 0x80u) != 0) {  // PB7 is T1's, whatever DDRB bit 7 says
        o.pbDrive |= 0x80u;
        o.pbLevel = static_cast<std::uint8_t>((orb_ & 0x7Fu) | (pb7_ ? 0x80u : 0u));
        o.pb7Timer = true;
    }
    if (ca2Mode() >= 4) {
        o.ca2Drive = true;
        o.ca2 = C2Level(ca2Mode(), ca2Out_);
    }
    if (cb1IsOutput()) {
        o.cb1Drive = true;
        o.cb1 = cb1Out_;
    }
    if (srMode() >= 4) {
        o.cb2Drive = true;
        o.cb2 = srCb2_;
        o.cb2Shift = true;
    } else if (srMode() == 0 && cb2Mode() >= 4) {
        o.cb2Drive = true;
        o.cb2 = C2Level(cb2Mode(), cb2Out_);
    }
    o.irq = (ifr_ & ier_ & 0x7Fu) != 0;
    return o;
}

}  // namespace bw
