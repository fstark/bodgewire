#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace bw {

enum class Lane : std::uint8_t { Machines, PostIts };

// The launcher's selection (P7b): a Machine, and in it a post-it (0: "Start the Machine", p + 1:
// Puzzle p). The top lane always has post-it 0.
class LauncherLanes {
public:
    // Each Machine's Puzzle count, in list order. The selection is clamped (a list read anew).
    void setPuzzles(std::vector<std::size_t> counts);
    void left();   // the previous Machine (post-it 0) or post-it; the ends stop
    void right();  // the next one
    void down();   // to the post-its, the post-it kept
    void up();     // to the Machines, post-it 0
    // Clicks: a Machine (top lane, post-it 0), or a post-it of the selected Machine (bottom lane;
    // post-it 0 too). Out of range: clamped.
    void selectMachine(std::size_t machine);
    void selectPostIt(std::size_t postIt);
    Lane lane() const { return lane_; }
    std::size_t machine() const { return machine_; }
    std::size_t postIt() const { return postIt_; }
    std::optional<std::size_t> puzzle() const;  // postIt() - 1; nullopt for "Start the Machine"

private:
    std::size_t postIts() const;  // of the selected Machine: its Puzzles and "Start the Machine"

    std::vector<std::size_t> puzzles_;
    Lane lane_ = Lane::Machines;
    std::size_t machine_ = 0, postIt_ = 0;
};

inline constexpr double kGlideTau = 0.064;  // s: e^(-0.25 / 0.064) ≈ 2 % left after 0.25 s

// A value easing towards its target (the lanes' scroll, P7b): exponential, 98 % of the way in a
// quarter of a second; snapped to the target when within `snap` (in the value's units: 0.002 of a
// Machine on the top lane, 0.5 px on the post-its' row).
struct Glide {
    double value = 0, target = 0;
    double snap = 0.002;
    void jump(double to);         // no glide (the start, coming back from a bench)
    void update(double seconds);  // value += (target - value) * (1 - exp(-seconds / kGlideTau))
    bool moving() const { return value != target; }
};

// The top lane's anchors, as shares of the lane's width: the selected picture centred in
// [kPictureFrom, kPictureTo]; the previous one's right edge at kHintLeft, the next one's left edge
// at kHintRight (P7b).
inline constexpr double kPictureFrom = 0.07, kPictureTo = 0.64, kHintLeft = 0.035,
                        kHintRight = 0.955;
// The x of a picture's left edge, as a share of the lane's width, for its distance `d` (its index
// minus the glide's value) and its width `w` (a share): piecewise linear through the three anchors
// (d = -1: right edge at kHintLeft; d = 0: centred; d = 1: left edge at kHintRight), extended
// linearly past them.
double PictureLeft(double d, double w);

}  // namespace bw
