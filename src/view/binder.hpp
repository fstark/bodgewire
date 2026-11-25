#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bw {

// The manuals binder's dividers, front to back (P6e); Contents once a memory chip's window was
// clicked (P6p); GiveUp on a Random fault's bench only (P8d).
enum class Divider : std::uint8_t { User, Service, Datasheets, Repairs, Contents, GiveUp };

// The top of each of `count` tabs down the binder's edge, the first at `top`: `step` apart, closer
// when the last, `tabHeight` tall, would end past `bottom` (overlapping in a binder too short for
// them all). Only the tabs that show are counted: a hidden one leaves no gap.
std::vector<float> TabTops(std::size_t count, float top, float bottom, float tabHeight, float step);

// The binder's height for its width (1: its proportions), the range a drag or --binder-height
// may stretch it to.
inline constexpr float kMinStretch = 0.7f, kMaxStretch = 2.5f;

// Which divider is open and which Part's sheet the index opened.
class Binder {
public:
    // Every frame, before drawing, out or away: `out` its window shown this frame, `selected`
    // the Board's selected Component. Coming out: the divider `show` asked for, else the User
    // Manual, the selection taken as seen (no turn), the opened Part closed. Out, a selection
    // different from the last frame's (not a deselection) turns to Datasheets and closes the
    // opened Part. Away, the selection is only remembered. `hasContents` (the selected Component
    // is a memory chip): a new selection also makes the Contents divider show it, its tab there
    // without the chip's small window clicked, and turns nothing more (P8f).
    void frame(bool out, std::optional<std::size_t> selected, bool hasContents = false);
    Divider divider() const { return divider_; }
    void turn(Divider divider) { divider_ = divider; }  // its tab clicked
    void show(Divider divider) { shown_ = divider; }    // --manual: the next coming out
    const std::optional<std::string>& part() const { return part_; }
    void openPart(std::string part) { part_ = std::move(part); }  // a Part's name in the index
    void closePart() { part_.reset(); }                           // All datasheets
    // A memory chip's window clicked (P6p): the Contents divider shows it, at once if the binder
    // is out, else when it comes out (the click brings it out); a later coming out from the shelf
    // is at the User Manual again.
    void openContents(std::size_t component);
    std::optional<std::size_t> contents() const { return contents_; }  // its tab shows once set

private:
    Divider divider_ = Divider::User;
    std::optional<Divider> shown_;
    std::optional<std::size_t> selected_;
    std::optional<std::string> part_;
    std::optional<std::size_t> contents_;
    bool out_ = false;
};

}  // namespace bw
