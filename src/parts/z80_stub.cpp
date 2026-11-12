#include "parts/z80_stub.hpp"

#include <string_view>
#include <utility>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;
using Kind = Z80Stub::Kind;

constexpr std::string_view kTitle = " BODGEWIRE P5A  BUS LOAD  SPIKE ";
constexpr std::uint8_t kNewline = 0x76;  // HALT
constexpr std::uint16_t kDisplayFile = 0x4000, kDisplayMirror = 0xC000;
constexpr std::uint16_t kStack = 0x43FE, kInterrupt = 0x0038, kPort = 0xFFFE;
constexpr std::uint16_t kCodeFirst = 0x0100, kCodeEnd = 0x0600;
constexpr int kVsyncLines = 4, kDisplayLines = Z80Stub::kRows * 8;

std::vector<PinSpec> Z80StubPins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto io = [](const char* name) {
        return PinSpec{name, PinKind::Bidirectional, Family::Ttl};
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {out("A11"),    out("A12"),    out("A13"),  out("A14"),    out("A15"),     in("CLK"),
            io("D4"),      io("D3"),      io("D5"),    io("D6"),      power("VCC"),   io("D2"),
            io("D7"),      io("D0"),      io("D1"),    in("INT_N"),   in("NMI_N"),    out("HALT_N"),
            out("MREQ_N"), out("IORQ_N"), out("RD_N"), out("WR_N"),   out("BUSAK_N"), in("WAIT_N"),
            in("BUSRQ_N"), in("RESET_N"), out("M1_N"), out("RFSH_N"), power("GND"),   out("A0"),
            out("A1"),     out("A2"),     out("A3"),   out("A4"),     out("A5"),      out("A6"),
            out("A7"),     out("A8"),     out("A9"),   out("A10")};
}

constexpr int Length(Kind kind) {
    switch (kind) {
    case Kind::Read:
    case Kind::Write:
        return 3;
    case Kind::InterruptAcknowledge:
        return 6;
    case Kind::Internal:
        return 1;
    default:
        return 4;
    }
}

constexpr bool IsM1(Kind kind) {
    return kind == Kind::Fetch || kind == Kind::HaltFetch || kind == Kind::InterruptAcknowledge;
}

// The fixed frame (design, Script).
class FrameBuilder {
public:
    std::vector<Z80Stub::Cycle> build() {
        for (int line = 0; line < Z80Stub::kFrameLines; ++line) {
            line_ = line;
            t_ = 0;
            const int display = line - Z80Stub::kFirstDisplayLine;
            if (line < kVsyncLines) {
                if (line == 0) {
                    add(Kind::In, kPort);
                }
                for (int i = 0; i < 29; ++i) {
                    fetch();
                    read();
                }
                if (line == kVsyncLines - 1) {
                    add(Kind::Out, kPort, 0xFF);
                } else if (line != 0) {
                    fetch();
                }
            } else if (display >= 0 && display < kDisplayLines) {
                const auto row = static_cast<std::uint16_t>(display / 8 * Z80Stub::kRowBytes);
                for (int column = 0; column <= Z80Stub::kColumns; ++column) {
                    add(Kind::Fetch, static_cast<std::uint16_t>(kDisplayMirror + row + column));
                }
                const auto halted =
                    static_cast<std::uint16_t>(kDisplayMirror + row + Z80Stub::kRowBytes);
                for (int i = 0; i < 4; ++i) {
                    add(Kind::HaltFetch, halted);
                }
                tail(halted);
            } else {
                for (int i = 0; i < 10; ++i) {
                    fetch();
                    read();
                    fetch();
                    const std::size_t cell = cursor_;
                    cursor_ = (cursor_ + 1) % Z80Stub::Page().size();
                    add(Kind::Write, static_cast<std::uint16_t>(kDisplayFile + cell),
                        Z80Stub::Page()[cell]);
                }
                fetch();
                fetch();
                tail(pc_);
            }
            Check(t_ == Z80Stub::kLineTStates, "Z80STUB: a line is not 207 T-states");
        }
        return std::move(frame_);
    }

private:
    void add(Kind kind, std::uint16_t address, std::uint8_t data = 0) {
        frame_.push_back({kind, address, data, static_cast<std::uint16_t>(line_),
                          static_cast<std::uint16_t>(t_)});
        t_ += Length(kind);
    }
    void fetch() {
        add(Kind::Fetch, pc_);
        pc_ = static_cast<std::uint16_t>(pc_ + 1 == kCodeEnd ? kCodeFirst : pc_ + 1);
    }
    void read() {
        lfsr_ = static_cast<std::uint16_t>((lfsr_ >> 1) ^ ((lfsr_ & 1) != 0 ? 0xB400 : 0));
        add(Kind::Read, static_cast<std::uint16_t>(kDisplayFile | (lfsr_ & 0x3FF)));
    }
    // From T-state 148: the interrupt that ends a line and its ROM routine.
    void tail(std::uint16_t pc) {
        Check(t_ == 148, "Z80STUB: the interrupt is not at T-state 148");
        add(Kind::InterruptAcknowledge, pc);
        add(Kind::Internal, pc);
        add(Kind::Write, kStack + 1, static_cast<std::uint8_t>(pc >> 8));
        add(Kind::Write, kStack, static_cast<std::uint8_t>(pc & 0xFF));
        for (int i = 0; i < 10; ++i) {
            add(Kind::Fetch, static_cast<std::uint16_t>(kInterrupt + i));
        }
        add(Kind::Read, kStack);
        add(Kind::Read, kStack + 1);
    }

    std::vector<Z80Stub::Cycle> frame_;
    int line_ = 0, t_ = 0;
    std::uint16_t pc_ = kCodeFirst;
    std::uint16_t lfsr_ = 0xACE1;
    std::size_t cursor_ = 0;
};

}  // namespace

const std::array<std::uint8_t, Z80Stub::kRows * Z80Stub::kRowBytes>& Z80Stub::Page() {
    static const auto page = [] {
        std::array<std::uint8_t, kRows * kRowBytes> bytes{};
        for (int row = 0; row < kRows; ++row) {
            for (int column = 0; column < kColumns; ++column) {
                unsigned code = 0;
                if (row == 0) {
                    code = static_cast<unsigned char>(kTitle[static_cast<std::size_t>(column)]) &
                           0x3Fu;
                } else if (row >= 2) {
                    code = static_cast<unsigned>(row * kColumns + column) & 0x3Fu;
                    code |= (row / 2) % 2 == 1 ? 0x80u : 0u;
                } else {
                    code = ' ' & 0x3F;
                }
                bytes[static_cast<std::size_t>(row * kRowBytes + column)] =
                    static_cast<std::uint8_t>(code);
            }
            bytes[static_cast<std::size_t>(row * kRowBytes + kColumns)] = kNewline;
        }
        return bytes;
    }();
    static_assert(kTitle.size() == kColumns);
    return page;
}

Z80Stub::Z80Stub(std::string reference)
    : Component(std::move(reference), Z80StubPins()), frame_(FrameBuilder().build()) {}

void Z80Stub::powerOn(PowerOnContext& ctx) {
    cycle_ = 0;
    t_ = -1;
    r_ = 0;
    dataDriven_ = false;
    for (const PinIndex pin : kAddress) {
        ctx.setOutput(pin, Zero);
    }
    for (const PinIndex pin : {kHaltN, kMreqN, kIorqN, kRdN, kWrN, kBusakN, kM1N, kRfshN}) {
        ctx.setOutput(pin, One);
    }
}

int Z80Stub::line() const { return frame_[cycle_].line; }

int Z80Stub::tState() const { return frame_[cycle_].start + (t_ < 0 ? 0 : t_); }

void Z80Stub::evaluate(EvalContext& ctx) {
    if (ctx.rose(kClk)) {
        rise(ctx);
    } else if (ctx.fell(kClk)) {
        fall(ctx);
    }
}

void Z80Stub::driveAddress(EvalContext& ctx, std::uint16_t address) {
    for (std::size_t bit = 0; bit < kAddress.size(); ++bit) {
        ctx.drive(kAddress[bit], (address >> bit) & 1 ? One : Zero, delays::kZ80Address);
    }
}

void Z80Stub::driveData(EvalContext& ctx, std::uint8_t data) {
    for (std::size_t bit = 0; bit < kData.size(); ++bit) {
        ctx.drive(kData[bit], (data >> bit) & 1 ? One : Zero, delays::kZ80Data);
    }
    dataDriven_ = true;
}

void Z80Stub::releaseData(EvalContext& ctx) {
    for (const PinIndex pin : kData) {
        ctx.drive(pin, Z, delays::kZ80Data);
    }
    dataDriven_ = false;
}

void Z80Stub::rise(EvalContext& ctx) {
    if (t_ < 0) {
        t_ = 0;
    } else if (++t_ == Length(frame_[cycle_].kind)) {
        t_ = 0;
        cycle_ = cycle_ + 1 == frame_.size() ? 0 : cycle_ + 1;
    }
    const Cycle& c = frame_[cycle_];
    const auto refresh = [&] {
        driveAddress(ctx, static_cast<std::uint16_t>(kRefreshPage | r_));
        ctx.drive(kRfshN, Zero, delays::kZ80Rfsh);
        r_ = static_cast<std::uint8_t>((r_ + 1) & 0x7F);
    };
    if (t_ == 0) {
        if (dataDriven_) {
            releaseData(ctx);
        }
        ctx.drive(kRfshN, One, delays::kZ80Rfsh);
        if (c.kind == Kind::Internal) {
            ctx.drive(kHaltN, One, delays::kZ80Halt);
            return;
        }
        driveAddress(ctx, c.address);
        if (IsM1(c.kind)) {
            ctx.drive(kM1N, Zero, delays::kZ80M1);
        }
        if (c.kind == Kind::HaltFetch) {
            ctx.drive(kHaltN, Zero, delays::kZ80Halt);
        }
        return;
    }
    switch (c.kind) {
    case Kind::Fetch:
    case Kind::HaltFetch:
        if (t_ == 2) {
            ctx.drive(kMreqN, One, delays::kZ80Mreq);
            ctx.drive(kRdN, One, delays::kZ80Rd);
            ctx.drive(kM1N, One, delays::kZ80M1);
            refresh();
        }
        break;
    case Kind::InterruptAcknowledge:
        if (t_ == 4) {
            ctx.drive(kM1N, One, delays::kZ80M1);
            ctx.drive(kIorqN, One, delays::kZ80Iorq);
            refresh();
        }
        break;
    case Kind::In:
    case Kind::Out:
        if (t_ == 1) {
            ctx.drive(kIorqN, Zero, delays::kZ80Iorq);
            if (c.kind == Kind::In) {
                ctx.drive(kRdN, Zero, delays::kZ80Rd);
            } else {
                ctx.drive(kWrN, Zero, delays::kZ80Wr);
            }
        }
        break;
    default:
        break;
    }
}

void Z80Stub::fall(EvalContext& ctx) {
    if (t_ < 0) {
        return;
    }
    const Cycle& c = frame_[cycle_];
    switch (c.kind) {
    case Kind::Fetch:
    case Kind::HaltFetch:
        if (t_ == 0) {
            ctx.drive(kMreqN, Zero, delays::kZ80Mreq);
            ctx.drive(kRdN, Zero, delays::kZ80Rd);
        } else if (t_ == 2) {
            ctx.drive(kMreqN, Zero, delays::kZ80Mreq);
        } else if (t_ == 3) {
            ctx.drive(kMreqN, One, delays::kZ80Mreq);
        }
        break;
    case Kind::Read:
        if (t_ == 0) {
            ctx.drive(kMreqN, Zero, delays::kZ80Mreq);
            ctx.drive(kRdN, Zero, delays::kZ80Rd);
        } else if (t_ == 2) {
            ctx.drive(kMreqN, One, delays::kZ80Mreq);
            ctx.drive(kRdN, One, delays::kZ80Rd);
        }
        break;
    case Kind::Write:
        if (t_ == 0) {
            ctx.drive(kMreqN, Zero, delays::kZ80Mreq);
            driveData(ctx, c.data);
        } else if (t_ == 1) {
            ctx.drive(kWrN, Zero, delays::kZ80Wr);
        } else if (t_ == 2) {
            ctx.drive(kMreqN, One, delays::kZ80Mreq);
            ctx.drive(kWrN, One, delays::kZ80Wr);
        }
        break;
    case Kind::In:
        if (t_ == 3) {
            ctx.drive(kIorqN, One, delays::kZ80Iorq);
            ctx.drive(kRdN, One, delays::kZ80Rd);
        }
        break;
    case Kind::Out:
        if (t_ == 0) {
            driveData(ctx, c.data);
        } else if (t_ == 3) {
            ctx.drive(kIorqN, One, delays::kZ80Iorq);
            ctx.drive(kWrN, One, delays::kZ80Wr);
        }
        break;
    case Kind::InterruptAcknowledge:
        if (t_ == 2) {
            ctx.drive(kIorqN, Zero, delays::kZ80Iorq);
        } else if (t_ == 4) {
            ctx.drive(kMreqN, Zero, delays::kZ80Mreq);
        } else if (t_ == 5) {
            ctx.drive(kMreqN, One, delays::kZ80Mreq);
        }
        break;
    default:
        break;
    }
}

}  // namespace bw
