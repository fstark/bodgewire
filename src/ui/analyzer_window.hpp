#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "instruments/logic_analyzer.hpp"
#include "session/session.hpp"

namespace bw {

class BoardView;

// Runs of equal value of some Channels of a Capture, from its first sampled point.
struct CaptureRuns {
    std::vector<std::uint32_t> start;    // point where run r begins; run r ends at start[r + 1]
    std::vector<std::uint32_t> value;    // bit b: channels[b]; 0 when unknown
    std::vector<std::uint8_t> unknown;   // a Channel is X
    std::size_t end = 0;                 // points in the Capture
    std::size_t at(double point) const;  // run holding `point` (clamped)
};

CaptureRuns RunsOf(const Capture& capture, const std::vector<std::size_t>& channels);

// The floating Logic Analyzer: settings, Channel Group and Channel rows, the last Capture's
// waveforms on one canvas.
class AnalyzerWindow {
public:
    void draw(bool& open, Session& session, BoardView& board);

private:
    struct Row {
        std::optional<std::size_t> group;  // a group's row, or a member's
        std::size_t channel = 0;           // Channel rows and members
        bool member = false;
    };
    struct PatternEdit {
        std::array<char, 16> text{};
        bool refused = false;
    };

    void fit() { from_ = 0, to_ = kDepth; }
    std::vector<Row> rows(const Session& session) const;
    const CaptureRuns& runs(const std::vector<std::size_t>& channels);

    std::optional<Capture> shown_;   // kept across rebuilds and power-off
    double from_ = 0, to_ = kDepth;  // visible points
    std::optional<double> marker_;   // point
    float pressX_ = 0;               // canvas press, to tell a click from a drag
    bool moved_ = false;
    std::set<std::size_t> selected_;                        // Channels picked for a hand group
    std::set<std::string> expanded_;                        // group names shown with their members
    std::map<std::vector<std::size_t>, CaptureRuns> runs_;  // for shown_
    std::map<std::string, PatternEdit> patterns_;           // per group name
    std::string editing_;                                   // group whose pattern is active
};

}  // namespace bw
