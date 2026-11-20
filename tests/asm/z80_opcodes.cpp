#include "z80_opcodes.hpp"

#include "asm/z80_disassembler.hpp"

namespace bw {

std::vector<std::vector<std::uint8_t>> Z80DocumentedOpcodes() {
    std::vector<std::vector<std::uint8_t>> out;
    const auto add = [&](std::vector<std::uint8_t> candidate) {
        if (const auto t = DisassembleZ80(candidate, 0x0100)) {
            candidate.resize(t->length);
            out.push_back(std::move(candidate));
        }
    };
    for (unsigned op = 0; op < 256; ++op) {
        const auto b = static_cast<std::uint8_t>(op);
        if (b != 0xCB && b != 0xDD && b != 0xED && b != 0xFD) {
            add({b, 0x34, 0x12});
        }
        add({0xCB, b});
        add({0xED, b, 0x34, 0x12});
        for (const std::uint8_t index : {std::uint8_t{0xDD}, std::uint8_t{0xFD}}) {
            if (b != 0xCB) {
                add({index, b, 0xFD, 0x5A, 0x12});
            }
            add({index, 0xCB, 0xFD, b});
        }
    }
    return out;
}

}  // namespace bw
