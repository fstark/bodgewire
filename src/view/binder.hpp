#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace bw {

// The manuals binder's dividers, front to back (P6e).
enum class Divider : std::uint8_t { User, Service, Datasheets, Repairs };

// Which divider is open and which Part's sheet the index opened.
class Binder {
public:
    // Every frame, before drawing, out or away: `out` its window shown this frame, `selected`
    // the Board's selected Component. Coming out: the divider `show` asked for, else the User
    // Manual, the selection taken as seen (no turn), the opened Part closed. Out, a selection
    // different from the last frame's (not a deselection) turns to Datasheets and closes the
    // opened Part. Away, the selection is only remembered.
    void frame(bool out, std::optional<std::size_t> selected);
    Divider divider() const { return divider_; }
    void turn(Divider divider) { divider_ = divider; }  // its tab clicked
    void show(Divider divider) { shown_ = divider; }    // --manual: the next coming out
    const std::optional<std::string>& part() const { return part_; }
    void openPart(std::string part) { part_ = std::move(part); }  // a Part's name in the index
    void closePart() { part_.reset(); }                           // All datasheets

private:
    Divider divider_ = Divider::User;
    std::optional<Divider> shown_;
    std::optional<std::size_t> selected_;
    std::optional<std::string> part_;
    bool out_ = false;
};

}  // namespace bw
