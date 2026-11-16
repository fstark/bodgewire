#include "bw80_screen.hpp"

#include <array>

#include <catch2/catch_test_macros.hpp>

#include "parts/ram2114.hpp"

namespace bw {

namespace {

const Ram2114& Ram(const LoadedCircuit& loaded, const char* reference) {
    const auto* ram = dynamic_cast<const Ram2114*>(loaded.components.at(reference));
    REQUIRE(ram != nullptr);
    return *ram;
}

// IO1-IO4 of IC3 are MD7, MD1, MD0, MD2; of IC4 MD4, MD3, MD5, MD6 (netlists/bw80.bwn).
constexpr std::array<int, 4> kIc3Bits{7, 1, 0, 2};
constexpr std::array<int, 4> kIc4Bits{4, 3, 5, 6};

std::vector<std::uint8_t> Codes(const LoadedCircuit& loaded,
                                std::vector<std::vector<std::uint8_t>>& rows) {
    const std::uint16_t dfile =
        static_cast<std::uint16_t>(Bw80Ram(loaded, 0x400C) | Bw80Ram(loaded, 0x400D) << 8);
    std::vector<std::uint8_t> all;
    std::uint16_t at = dfile;
    REQUIRE(Bw80Ram(loaded, at) == 0x76);  // the leading HALT
    ++at;
    rows.clear();
    for (int row = 0; row < kBw80Rows && at < dfile + 1024; ++row) {
        std::vector<std::uint8_t> codes;
        while (at < dfile + 1024) {
            const std::uint8_t code = Bw80Ram(loaded, at++);
            if (code == 0x76) {
                break;
            }
            codes.push_back(code);
        }
        rows.push_back(codes);
    }
    return all;
}

char Ascii(std::uint8_t code) {
    static constexpr std::string_view kSymbols = " \"";
    static constexpr std::string_view kPunct = "()-+*/=><;,.";  // $10-$1B
    const std::uint8_t plain = code & 0x7F;
    if (plain < 0x02) {
        return kSymbols[plain];
    }
    if (plain < 0x0C) {
        return '#';
    }
    switch (plain) {
    case 0x0C:
        return 'L';  // the pound sign, kept ASCII
    case 0x0D:
        return '$';
    case 0x0E:
        return ':';
    case 0x0F:
        return '?';
    default:
        break;
    }
    if (plain >= 0x10 && plain <= 0x1B) {
        return kPunct[plain - 0x10];
    }
    if (plain >= 0x1C && plain <= 0x25) {
        return static_cast<char>('0' + plain - 0x1C);
    }
    if (plain >= 0x26 && plain <= 0x3F) {
        const char letter = static_cast<char>('A' + plain - 0x26);
        return (code & 0x80) != 0 ? static_cast<char>(letter - 'A' + 'a') : letter;
    }
    return '?';
}

}  // namespace

std::uint8_t Bw80Ram(const LoadedCircuit& loaded, std::uint16_t address) {
    const std::size_t word = address & 0x3FF;
    const std::uint8_t low = Ram(loaded, "IC3").word(word), high = Ram(loaded, "IC4").word(word);
    unsigned byte = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        byte |= ((low >> i) & 1u) << kIc3Bits[i];
        byte |= ((high >> i) & 1u) << kIc4Bits[i];
    }
    return static_cast<std::uint8_t>(byte);
}

std::vector<std::string> Bw80DisplayRows(const LoadedCircuit& loaded) {
    std::vector<std::vector<std::uint8_t>> rows;
    Codes(loaded, rows);
    std::vector<std::string> text;
    for (const auto& codes : rows) {
        std::string line;
        for (const std::uint8_t code : codes) {
            if (code >= 0xD4 && code != 0xFF) {
                line += "<" + std::to_string(code) + ">";
            } else {
                line.push_back(Ascii(code));
            }
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        text.push_back(line);
    }
    return text;
}

PageDots Bw80ExpectedPage(const LoadedCircuit& loaded, std::span<const std::uint8_t> glyphs) {
    std::vector<std::vector<std::uint8_t>> rows;
    Codes(loaded, rows);
    return ExpectedPage(glyphs, kBw80Columns, kBw80Rows, [&](int row, int column) -> unsigned {
        if (row >= static_cast<int>(rows.size()) ||
            column >= static_cast<int>(rows[static_cast<std::size_t>(row)].size())) {
            return 0;
        }
        return rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
    });
}

}  // namespace bw
