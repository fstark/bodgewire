#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
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

// Where the binder is open (P8j): a divider and, on Datasheets, the selected chip (nullopt: the
// list); on Contents, its chip. Compared without the scroll.
struct BinderPlace {
    Divider divider = Divider::User;
    std::optional<std::size_t> chip;
    bool operator==(const BinderPlace& o) const { return divider == o.divider && chip == o.chip; }
};

// At most this many places to go back to (P8j).
inline constexpr std::size_t kHistory = 32;

// Which divider is open, and the places it was open at before (Back and Forward, P8j).
class Binder {
public:
    // Every frame, before drawing, out or away: `out` its window shown this frame, `selected`
    // the Board's selected Component. Coming out: the divider `show` asked for, else the User
    // Manual, the selection taken as seen (no turn). Out, a selection different from the last
    // frame's (not a deselection) turns to Datasheets. Away, the selection is only remembered.
    // `hasContents` (the selected Component is a memory chip): a new selection also makes the
    // Contents divider show it, its tab there without the chip's small window clicked, and turns
    // nothing more (P8f). Out, a change of here() by a selection is a move: the place left is
    // kept for back() (U5 → U7 on Datasheets too); coming out is none.
    void frame(bool out, std::optional<std::size_t> selected, bool hasContents = false);
    Divider divider() const { return divider_; }
    void turn(Divider divider);                       // its tab clicked: a move
    void show(Divider divider) { shown_ = divider; }  // --manual: the next coming out
    BinderPlace here() const;
    // The page drawn this frame and its scroll, kept per place for back(): only the page drawn
    // writes its place's (a hovered chip's page never writes the selected one's).
    void scrolled(const BinderPlace& page, float y);
    bool canGoBack() const { return !history_.empty(); }
    bool canGoForward() const { return !ahead_.empty(); }
    // Back, as a browser's: the last place left, taken off the history, with its scroll; the
    // place it leaves goes ahead, for forward(). Forward: the last place gone back from, the one
    // it leaves back on the history. Any other move clears what is ahead. The binder is there at
    // once (its divider, its chip as the selection seen or as Contents' chip: the next frame()
    // sees no new selection); the caller makes the Board's selection match (select or deselect,
    // never framing) and scrolls the page once. nullopt with nowhere to go.
    struct Visit {
        BinderPlace place;
        float scroll = 0;
    };
    std::optional<Visit> back();
    std::optional<Visit> forward();
    // All chips: the Datasheets list, a move; the caller deselects the Board.
    void showList();
    // A memory chip's window clicked (P6p): the Contents divider shows it, at once if the binder
    // is out, else when it comes out (the click brings it out); a later coming out from the shelf
    // is at the User Manual again.
    void openContents(std::size_t component);
    std::optional<std::size_t> contents() const { return contents_; }  // its tab shows once set

private:
    void record(const BinderPlace& from);  // a move: kept for back(), but a repeat of the last
    Visit go(std::vector<BinderPlace>& from, std::vector<BinderPlace>& to);

    std::vector<BinderPlace> history_;
    std::vector<BinderPlace> ahead_;  // gone back from, the latest last
    std::map<std::pair<Divider, std::optional<std::size_t>>, float> scrolls_;
    Divider divider_ = Divider::User;
    std::optional<Divider> shown_;
    std::optional<std::size_t> selected_;
    std::optional<std::size_t> contents_;
    bool out_ = false;
};

}  // namespace bw
