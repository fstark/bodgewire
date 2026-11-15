#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "sim/component.hpp"
#include "sim/primitives.hpp"

namespace bw {

struct GateUnit {
    GateKind kind;
    std::uint8_t output;                 // package pin
    std::array<std::uint8_t, 8> inputs;  // package pins
    std::uint8_t inputCount;
};

struct GatePackageSpec {
    std::string_view part;
    std::uint8_t pinCount;
    std::span<const std::string_view> names;  // index = pin - 1; "GND", "VCC", "NC<n>" included
    std::span<const GateUnit> units;
    // EvaluatePackage<its units, its delays>: evaluation specialised on them at compile time
    // (gate_package.cpp)
    void (*evaluate)(EvalContext& ctx);
    bool openCollector = false;  // unit outputs are PinKind::OpenCollector (7417)
};

const GatePackageSpec& GatePackageSpecOf(std::string_view part);  // Check()s

// One chip of independent gates (74LS00, 02, 04, 08, 20, 21, 27, 30, 32, 86; 7417).
class GatePackage final : public Component {
public:
    GatePackage(std::string reference, const GatePackageSpec& spec);
    void evaluate(EvalContext& ctx) override;

private:
    const GatePackageSpec& spec_;
};

}  // namespace bw
