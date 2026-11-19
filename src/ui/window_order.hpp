#pragma once

namespace bw {

// The floating windows' order (P6c), by the name ImGui::Begin takes ("###id" works).
// True when `name` is the visible window drawn in front of every other floating one.
bool InFront(const char* name);
// In front, with ImGui's keyboard focus.
void BringToFront(const char* name);

}  // namespace bw
