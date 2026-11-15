#pragma once

#include <cstdint>

namespace bw {

enum class LogicValue : std::uint8_t { Zero, One, Z, X };  // Z only as a drive (S1)
// Port: an NMOS port's low output (P5q): beats a pull-up and loses to any output's high, as an
// LS output's high (tens of mA) beats a 6520/6522/6530 port's sink (about 2 mA).
enum class DriveStrength : std::uint8_t { None, Weak, Port, Strong, Supply };
enum class Family : std::uint8_t { Ttl, Mos };
// Switched: a Pin of the switch network (P5i): drives its Net at Supply strength, reads nothing
// through the Net's value; its Component is woken when the Net's other drives change.
// PullUp: an NMOS port pin (6530, P5j): drives 0 and X at Port strength, 1 Weak (its own pull-up);
// reads its Net.
// Joined: a Pin of the resistor network (P5u): drives its Net at Weak strength, reads nothing
// through the Net's value; its Component is woken when the Net's other drives change, as Switched.
enum class PinKind : std::uint8_t {
    Input,
    Output,
    OpenCollector,
    Bidirectional,
    Power,
    NoConnect,
    Switched,
    PullUp,
    Joined
};

// Drives its Net: an output of any kind, a bidirectional, switched, pull-up or joined Pin.
constexpr bool IsDriver(PinKind kind) {
    return kind == PinKind::Output || kind == PinKind::OpenCollector ||
           kind == PinKind::Bidirectional || kind == PinKind::Switched || kind == PinKind::PullUp ||
           kind == PinKind::Joined;
}

// A network's Pin (switch or resistor network): at most one on a Net, woken by the Net's other
// drives only.
constexpr bool IsNetworkPin(PinKind kind) {
    return kind == PinKind::Switched || kind == PinKind::Joined;
}

// Reads its Net: an input, a bidirectional or a pull-up Pin.
constexpr bool IsReader(PinKind kind) {
    return kind == PinKind::Input || kind == PinKind::Bidirectional || kind == PinKind::PullUp;
}

struct Drive {
    LogicValue value;
    DriveStrength strength;
    Family family;
};

constexpr LogicValue Not(LogicValue value) {
    switch (value) {
    case LogicValue::Zero:
        return LogicValue::One;
    case LogicValue::One:
        return LogicValue::Zero;
    default:
        return LogicValue::X;
    }
}

constexpr char ToChar(LogicValue value) {
    switch (value) {
    case LogicValue::Zero:
        return '0';
    case LogicValue::One:
        return '1';
    case LogicValue::Z:
        return 'z';
    default:
        return 'x';
    }
}

// Output Pin value → its Drive: totem-pole Z → None; open collector 1/Z → None; switched: Supply;
// pull-up: 0 and X Port, 1 Weak, Z → None; joined: Weak.
constexpr Drive DriveOf(PinKind kind, Family family, LogicValue value) {
    switch (kind) {
    case PinKind::Output:
    case PinKind::Bidirectional:
        return {value, value == LogicValue::Z ? DriveStrength::None : DriveStrength::Strong,
                family};
    case PinKind::Switched:
        return {value, value == LogicValue::Z ? DriveStrength::None : DriveStrength::Supply,
                family};
    case PinKind::Joined:
        return {value, value == LogicValue::Z ? DriveStrength::None : DriveStrength::Weak, family};
    case PinKind::PullUp:
        if (value == LogicValue::Z) {
            return {value, DriveStrength::None, family};
        }
        return {value, value == LogicValue::One ? DriveStrength::Weak : DriveStrength::Port,
                family};
    case PinKind::OpenCollector:
        if (value == LogicValue::Zero || value == LogicValue::X) {
            return {value, DriveStrength::Strong, family};
        }
        return {LogicValue::Z, DriveStrength::None, family};
    default:
        return {LogicValue::Z, DriveStrength::None, family};
    }
}

struct Resolution {
    LogicValue value;  // never Z
    bool floating;
};

// Accumulates the Drives of one Net, then applies the S2 table.
class Resolver {
public:
    void add(Drive drive) {
        if (drive.strength == DriveStrength::None) {
            return;
        }
        if (drive.strength > max_) {
            max_ = drive.strength;
            value_ = drive.value;
            conflict_ = false;
            sawX_ = drive.value == LogicValue::X;
            allTtl_ = drive.family == Family::Ttl;
            return;
        }
        if (drive.strength == max_) {
            conflict_ |= drive.value != value_;
            sawX_ |= drive.value == LogicValue::X;
            allTtl_ &= drive.family == Family::Ttl;
        }
    }

    // The same rules as add() over everything `other` accumulated.
    void merge(const Resolver& other) {
        if (other.max_ == DriveStrength::None) {
            return;
        }
        if (other.max_ > max_) {
            *this = other;
            return;
        }
        if (other.max_ == max_) {
            conflict_ |= other.conflict_ || other.value_ != value_;
            sawX_ |= other.sawX_;
            allTtl_ &= other.allTtl_;
        }
    }

    DriveStrength strength() const { return max_; }
    bool operator==(const Resolver&) const = default;

    Resolution finish(bool hasTtlInput, LogicValue lastDriven) const;

private:
    DriveStrength max_ = DriveStrength::None;
    LogicValue value_ = LogicValue::Z;
    bool conflict_ = false;
    bool sawX_ = false;
    bool allTtl_ = true;
};

}  // namespace bw
