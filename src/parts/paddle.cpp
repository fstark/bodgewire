#include "parts/paddle.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

#include "parts/connector.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr double kMaxDuration = 1e18;  // ps; keeps llround in range

struct DurationUnit {
    std::string_view name;
    double picos;
};

constexpr std::array<DurationUnit, 5> kDurationUnits{
    {{"ps", 1}, {"ns", 1e3}, {"us", 1e6}, {"ms", 1e9}, {"s", 1e12}}};

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::expected<Time, std::string> DurationFromText(std::string_view text) {
    const auto bad = [&] {
        return std::unexpected("bad duration '" + std::string(text) +
                               "': expected a number then ps, ns, us, ms or s, e.g. 10ms");
    };
    std::size_t i = 0;
    double value = 0;
    for (; i < text.size() && IsDigit(text[i]); ++i) {
        value = value * 10 + (text[i] - '0');
    }
    if (i == 0) {
        return bad();
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        double scale = 0.1;
        const std::size_t start = i;
        for (; i < text.size() && IsDigit(text[i]); ++i) {
            value += (text[i] - '0') * scale;
            scale /= 10;
        }
        if (i == start) {
            return bad();
        }
    }
    const std::string_view unit = text.substr(i);
    for (const DurationUnit& candidate : kDurationUnits) {
        if (candidate.name != unit) {
            continue;
        }
        const double picos = value * candidate.picos;
        if (picos > kMaxDuration) {
            return std::unexpected("duration too long");
        }
        const Time rounded = std::llround(picos);
        if (rounded < 1) {
            return std::unexpected("duration must be >= 1 ps");
        }
        return rounded;
    }
    return bad();
}

Paddle::Paddle(std::string reference, Time shortest, Time longest)
    : Component(std::move(reference), ConnectorPins("PADDLE")), shortest_(shortest),
      longest_(longest) {
    Check(shortest >= 1 && shortest < longest, "paddle: need 1 ps <= shortest < longest");
}

void Paddle::setKnob(double turn) {
    knob_.store(std::clamp(turn, 0.0, 1.0), std::memory_order_relaxed);
}

Time Paddle::pulseLength() const {
    return shortest_ + std::llround(knob() * static_cast<double>(longest_ - shortest_));
}

void Paddle::powerOn(PowerOnContext& ctx) {
    busy_ = false;
    endsAt_ = 0;
    ctx.setOutput(kOut, Zero);
}

void Paddle::evaluate(EvalContext& ctx) {
    const bool ending = busy_ && ctx.now() >= endsAt_;
    if (ending) {
        busy_ = false;
    }
    const bool started = !busy_ && ctx.fell(kTrigN);
    if (started) {
        const Time length = pulseLength();
        busy_ = true;
        endsAt_ = ctx.now() + length;
        ctx.wakeAfter(length);
    }
    if (ending || started) {
        ctx.drive(kOut, busy_ ? One : Zero, kPaddleDelay);
    }
}

}  // namespace bw
