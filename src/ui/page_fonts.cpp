#include "ui/page_fonts.hpp"

#include <array>
#include <cstddef>

namespace bw {

// Embedded by src/ui/CMakeLists.txt (page_fonts_data.cpp.in).
extern const unsigned char kPageTextFont[];
extern const std::size_t kPageTextFontSize;
extern const unsigned char kPageBoldFont[];
extern const std::size_t kPageBoldFontSize;
extern const unsigned char kPageMonoFont[];
extern const std::size_t kPageMonoFontSize;

namespace {

std::array<ImFont*, 3> gFaces{};  // by PageFace

}  // namespace

void LoadPageFonts(ImFontAtlas& fonts) {
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;  // static data, never written
    const auto add = [&](const unsigned char* data, std::size_t size) {
        return fonts.AddFontFromMemoryTTF(const_cast<unsigned char*>(data), static_cast<int>(size),
                                          0.0f, &config);
    };
    gFaces = {add(kPageTextFont, kPageTextFontSize), add(kPageBoldFont, kPageBoldFontSize),
              add(kPageMonoFont, kPageMonoFontSize)};
}

void PushPageFont(PageFace face, float scale) {
    ImGui::PushFont(gFaces[static_cast<std::size_t>(face)], kPageFontSize * scale);
}

}  // namespace bw
