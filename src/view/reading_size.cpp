#include "view/reading_size.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <system_error>
#include <vector>

#include "text/text.hpp"

namespace bw {

int NextReadingSize(int size) {
    const auto at = std::ranges::find(kReadingSizes, size);
    return at == kReadingSizes.end() || at + 1 == kReadingSizes.end() ? kReadingSizes.front()
                                                                      : *(at + 1);
}

bool IsReadingSize(int size) { return std::ranges::contains(kReadingSizes, size); }

std::string SettingsText(int readingSize) {
    return std::format("# Bodgewire settings\ntext-size {}\n", readingSize);
}

int ParseReadingSize(std::string_view settings) {
    int size = kReadingSizes.front();
    for (const std::string_view line : Split(settings, '\n')) {
        const std::vector<std::string_view> words = SplitWords(Trim(line));
        if (words.size() == 2 && words[0] == "text-size") {
            int value = 0;
            const auto [end, error] =
                std::from_chars(words[1].data(), words[1].data() + words[1].size(), value);
            const bool whole = error == std::errc{} && end == words[1].data() + words[1].size();
            size = whole && IsReadingSize(value) ? value : kReadingSizes.front();
        }
    }
    return size;
}

}  // namespace bw
