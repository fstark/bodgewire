#include "parts/gate_package.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

// 74LS00 / 08 / 32 / 86
constexpr std::array<std::string_view, 14> s_quadNames{"1A", "1B", "1Y", "2A", "2B", "2Y", "GND",
                                                       "3Y", "3A", "3B", "4Y", "4A", "4B", "VCC"};

constexpr std::array<GateUnit, 4> QuadUnits(GateKind kind) {
    return {{{kind, 3, {1, 2}, 2},
             {kind, 6, {4, 5}, 2},
             {kind, 8, {9, 10}, 2},
             {kind, 11, {12, 13}, 2}}};
}

constexpr auto s_ls00Units = QuadUnits(GateKind::Nand);
constexpr auto s_ls08Units = QuadUnits(GateKind::And);
constexpr auto s_ls32Units = QuadUnits(GateKind::Or);
constexpr auto s_ls86Units = QuadUnits(GateKind::Xor);

constexpr std::array<std::string_view, 14> s_ls02Names{"1Y", "1A", "1B", "2Y", "2A", "2B", "GND",
                                                       "3A", "3B", "3Y", "4A", "4B", "4Y", "VCC"};
constexpr std::array<GateUnit, 4> s_ls02Units{{{GateKind::Nor, 1, {2, 3}, 2},
                                               {GateKind::Nor, 4, {5, 6}, 2},
                                               {GateKind::Nor, 10, {8, 9}, 2},
                                               {GateKind::Nor, 13, {11, 12}, 2}}};

constexpr std::array<std::string_view, 14> s_ls04Names{"1A", "1Y", "2A", "2Y", "3A", "3Y", "GND",
                                                       "4Y", "4A", "5Y", "5A", "6Y", "6A", "VCC"};
constexpr std::array<GateUnit, 6> s_ls04Units{{{GateKind::Not, 2, {1}, 1},
                                               {GateKind::Not, 4, {3}, 1},
                                               {GateKind::Not, 6, {5}, 1},
                                               {GateKind::Not, 8, {9}, 1},
                                               {GateKind::Not, 10, {11}, 1},
                                               {GateKind::Not, 12, {13}, 1}}};

constexpr std::array<std::string_view, 14> s_ls20Names{
    "1A", "1B", "NC3", "1C", "1D", "1Y", "GND", "2Y", "2A", "2B", "NC11", "2C", "2D", "VCC"};
constexpr std::array<GateUnit, 2> s_ls20Units{
    {{GateKind::Nand, 6, {1, 2, 4, 5}, 4}, {GateKind::Nand, 8, {9, 10, 12, 13}, 4}}};

constexpr std::array<std::string_view, 14> s_ls27Names{"1A", "1B", "2A", "2B", "2C", "2Y", "GND",
                                                       "3Y", "3A", "3B", "3C", "1Y", "1C", "VCC"};
constexpr std::array<GateUnit, 3> s_ls27Units{{{GateKind::Nor, 12, {1, 2, 13}, 3},
                                               {GateKind::Nor, 6, {3, 4, 5}, 3},
                                               {GateKind::Nor, 8, {9, 10, 11}, 3}}};

constexpr std::array<std::string_view, 14> s_ls30Names{"A", "B",   "C",    "D", "E", "F",    "GND",
                                                       "Y", "NC9", "NC10", "G", "H", "NC13", "VCC"};
constexpr std::array<GateUnit, 1> s_ls30Units{{{GateKind::Nand, 8, {1, 2, 3, 4, 5, 6, 11, 12}, 8}}};

// The units and delays of one Part, known at compile time: the loops unroll and each gate becomes
// its own few instructions (its kind, its Pins and its delays are constants).
template <const auto& Units, const Delays& PartDelays>
void EvaluatePackage(EvalContext& ctx) {
    for (const GateUnit& unit : Units) {
        std::array<LogicValue, 8> inputs;
        for (std::uint8_t i = 0; i < unit.inputCount; ++i) {
            inputs[i] = ctx.read(unit.inputs[i] - 1u);
        }
        const LogicValue value = GateFunction(unit.kind, std::span(inputs.data(), unit.inputCount));
        ctx.drive(unit.output - 1u, value, DelayFor(PartDelays, value));
    }
}

constexpr std::array<GatePackageSpec, 9> s_specs{{
    {"74LS00", 14, s_quadNames, s_ls00Units, &EvaluatePackage<s_ls00Units, delays::kLs00>},
    {"74LS02", 14, s_ls02Names, s_ls02Units, &EvaluatePackage<s_ls02Units, delays::kLs02>},
    {"74LS04", 14, s_ls04Names, s_ls04Units, &EvaluatePackage<s_ls04Units, delays::kLs04>},
    {"74LS08", 14, s_quadNames, s_ls08Units, &EvaluatePackage<s_ls08Units, delays::kLs08>},
    {"74LS20", 14, s_ls20Names, s_ls20Units, &EvaluatePackage<s_ls20Units, delays::kLs20>},
    {"74LS27", 14, s_ls27Names, s_ls27Units, &EvaluatePackage<s_ls27Units, delays::kLs27>},
    {"74LS30", 14, s_ls30Names, s_ls30Units, &EvaluatePackage<s_ls30Units, delays::kLs30>},
    {"74LS32", 14, s_quadNames, s_ls32Units, &EvaluatePackage<s_ls32Units, delays::kLs32>},
    {"74LS86", 14, s_quadNames, s_ls86Units, &EvaluatePackage<s_ls86Units, delays::kLs86>},
}};

bool IsUnitOutput(const GatePackageSpec& spec, std::size_t pin) {
    for (const GateUnit& unit : spec.units) {
        if (unit.output == pin) {
            return true;
        }
    }
    return false;
}

std::vector<PinSpec> PackagePins(const GatePackageSpec& spec) {
    std::vector<PinSpec> pins;
    for (std::size_t i = 0; i < spec.names.size(); ++i) {
        const std::string_view name = spec.names[i];
        PinKind kind = PinKind::Input;
        if (name == "GND" || name == "VCC") {
            kind = PinKind::Power;
        } else if (name.starts_with("NC")) {
            kind = PinKind::NoConnect;
        } else if (IsUnitOutput(spec, i + 1)) {
            kind = PinKind::Output;
        }
        pins.push_back({std::string(name), kind, Family::Ttl});
    }
    return pins;
}

}  // namespace

const GatePackageSpec& GatePackageSpecOf(std::string_view part) {
    for (const GatePackageSpec& spec : s_specs) {
        if (spec.part == part) {
            return spec;
        }
    }
    Check(false, "gate package: unknown part " + std::string(part));
    return s_specs[0];
}

GatePackage::GatePackage(std::string reference, const GatePackageSpec& spec)
    : Component(std::move(reference), PackagePins(spec)), spec_(spec) {
    Check(spec.names.size() == spec.pinCount, "gate package: pin names do not match pin count");
}

void GatePackage::evaluate(EvalContext& ctx) { spec_.evaluate(ctx); }

}  // namespace bw
