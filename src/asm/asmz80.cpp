#include "asm/asmz80.hpp"

namespace bw::asm_detail {

namespace {

class Z80Set final : public InstructionSet {
public:
    bool knows(std::string_view) const override { return false; }
    void assemble(AsmContext&, std::string_view, std::string_view) const override {}
};

}  // namespace

const InstructionSet& Z80Instructions() {
    static const Z80Set set;
    return set;
}

}  // namespace bw::asm_detail
