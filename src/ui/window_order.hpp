#pragma once

#include <span>

namespace bw {

// The floating windows' order (P6c), by the name ImGui::Begin takes ("###id" works).
// True when `name` is the visible window drawn in front of every other floating one.
bool InFront(const char* name);
// True when `name` is visible and every floating window drawn in front of it is one of `group`
// (the Machine slot's objects, P6o).
bool InFrontOf(const char* name, std::span<const char* const> group);
// In front, with ImGui's keyboard focus.
void BringToFront(const char* name);

}  // namespace bw
