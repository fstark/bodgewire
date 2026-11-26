#include "ui/scope_case.hpp"

namespace bw {

void DrawScopeCase(ImDrawList& list, ImVec2 at, const ScopeCaseLayout& l, float width,
                   float height) {
    DrawCaseFrame(list, at, width, height);
    DrawGroup(list, at, l.timeGroup);
    DrawGroup(list, at, l.triggerGroup);
    const float labelY = l.timeGroup.y0 + 2;
    DrawGroupLabel(list, at, l.timeLess.x0, l.timeMore.x1, labelY, "TIME / DIV");
    DrawGroupLabel(list, at, l.positionLess.x0, l.positionMore.x1, labelY, "POSITION");
    DrawGroupLabel(list, at, l.autoHalf.x0, l.lamp.x1, labelY, "TRIGGER");
    DrawBadge(list, at, Badge{.logo = l.logo, .words = l.words, .hasWords = l.hasWords});
    DrawBezel(list, at, l.bezel);
}

}  // namespace bw
