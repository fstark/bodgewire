#include "view/matrix_keyboard_layout.hpp"

#include <array>
#include <string_view>
#include <vector>

#include "parts/matrix_keyboard.hpp"

namespace bw {

namespace {

struct Row {
    float y, x;                                            // the row's first cap
    std::vector<std::pair<std::string_view, float>> keys;  // legend, width (u)
};

std::vector<MatrixCap> MakeCaps() {
    // The two SHIFTs share a legend: the left one comes first in Bw2001Keys().
    const std::vector<Row> rows{
        {0,
         0,
         {{"!", 1},
          {"\"", 1},
          {"#", 1},
          {"$", 1},
          {"%", 1},
          {"'", 1},
          {"&", 1},
          {"\\", 1},
          {"(", 1},
          {")", 1},
          {"_", 1}}},
        {1,
         0,
         {{"RVS", 1},
          {"Q", 1},
          {"W", 1},
          {"E", 1},
          {"R", 1},
          {"T", 1},
          {"Y", 1},
          {"U", 1},
          {"I", 1},
          {"O", 1},
          {"P", 1},
          {"^", 1}}},
        {2,
         0.5f,
         {{"A", 1},
          {"S", 1},
          {"D", 1},
          {"F", 1},
          {"G", 1},
          {"H", 1},
          {"J", 1},
          {"K", 1},
          {"L", 1},
          {":", 1},
          {"RETURN", 1.5f}}},
        {3,
         0,
         {{"SHIFT", 1},
          {"Z", 1},
          {"X", 1},
          {"C", 1},
          {"V", 1},
          {"B", 1},
          {"N", 1},
          {"M", 1},
          {",", 1},
          {";", 1},
          {"?", 1},
          {"SHIFT", 1}}},
        {4, 0, {{"@", 1}, {"[", 1}, {"]", 1}, {"SPACE", 5}, {"<", 1}, {">", 1}, {"STOP", 1}}},
        {0, kMatrixPadX, {{"HOME", 1}, {"CRSR DN", 1}, {"CRSR RT", 1}, {"DEL", 1}}},
        {1, kMatrixPadX, {{"7", 1}, {"8", 1}, {"9", 1}, {"/", 1}}},
        {2, kMatrixPadX, {{"4", 1}, {"5", 1}, {"6", 1}, {"*", 1}}},
        {3, kMatrixPadX, {{"1", 1}, {"2", 1}, {"3", 1}, {"+", 1}}},
        {4, kMatrixPadX, {{"0", 1}, {".", 1}, {"-", 1}, {"=", 1}}},
    };
    const auto keys = Bw2001Keys();
    std::vector<MatrixCap> caps;
    std::vector<bool> used(keys.size(), false);
    for (const Row& row : rows) {
        float x = row.x;
        for (const auto& [legend, width] : row.keys) {
            for (std::size_t k = 0; k < keys.size(); ++k) {
                if (!used[k] && keys[k].legend == legend) {
                    used[k] = true;
                    caps.push_back({k, x, row.y, width});
                    break;
                }
            }
            x += width;
        }
    }
    return caps;
}

}  // namespace

std::span<const MatrixCap> Bw2001Caps() {
    static const std::vector<MatrixCap> caps = MakeCaps();
    return caps;
}

}  // namespace bw
