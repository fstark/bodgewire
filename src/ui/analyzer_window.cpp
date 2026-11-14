#include "ui/analyzer_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <string>

#include <imgui.h>

#include "ui/board_view.hpp"
#include "ui/toolbar.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr float kLaneHeight = 20.0f;
constexpr float kAxisHeight = 18.0f;
constexpr float kLabelWidth = 340.0f;
constexpr double kMinPoints = 16;
constexpr float kZoomStep = 0.8f;  // span factor per wheel notch
constexpr ImU32 kUnknown = IM_COL32(140, 140, 140, 110);
constexpr ImU32 kTriggerLine = IM_COL32(255, 255, 255, 140);
constexpr ImU32 kCursorLine = IM_COL32(255, 255, 255, 90);
constexpr ImU32 kMarkerLine = IM_COL32(255, 200, 0, 160);
constexpr ImU32 kAxis = IM_COL32(180, 180, 180, 255);
constexpr ImU32 kFaint = IM_COL32(200, 200, 200, 90);
constexpr ImU32 kBusText = IM_COL32(20, 20, 20, 255);
constexpr ImVec4 kGrey{0.5f, 0.5f, 0.5f, 1.0f};
constexpr ImVec4 kRefused{0.55f, 0.1f, 0.1f, 1.0f};
constexpr std::array<const char*, 5> kConditions{"any", "0", "1", "rise", "fall"};
constexpr std::array<const char*, 3> kPositions{"10 %", "50 %", "90 %"};
constexpr const char* kEmptyHint =
    "Clip (C), then click a chip pin. Shift-click: the pin's whole bus.";

const char* PhaseText(AnalyzerPhase phase, bool powered) {
    switch (phase) {
    case AnalyzerPhase::Idle:
        return "idle";
    case AnalyzerPhase::Armed:
        return powered ? "armed: waiting for trigger" : "armed: waiting for power";
    case AnalyzerPhase::Filling:
        return "triggered: filling";
    case AnalyzerPhase::Captured:
        return "captured";
    }
    return "";
}

// Time step in 1-2-5 steps giving at most `ticks` ticks over `span` ps.
Time TickStep(double span, int ticks) {
    Time step = 1;
    while (true) {
        for (const Time m : {1, 2, 5}) {
            if (span / static_cast<double>(step * m) <= ticks) {
                return step * m;
            }
        }
        step *= 10;
    }
}

// `width` bits in hex, X for a value with an unknown member.
std::string BusText(std::uint32_t value, bool unknown, std::size_t width) {
    const std::size_t digits = (width + 3) / 4;
    if (unknown) {
        return std::string(digits, 'X');
    }
    return std::format("{:0{}X}", value, digits);
}

}  // namespace

std::size_t CaptureRuns::at(double point) const {
    const auto p = static_cast<std::uint32_t>(std::clamp(point, 0.0, static_cast<double>(end - 1)));
    const auto it = std::ranges::upper_bound(start, p);
    return it == start.begin() ? 0 : static_cast<std::size_t>(it - start.begin()) - 1;
}

CaptureRuns RunsOf(const Capture& capture, const std::vector<std::size_t>& channels) {
    CaptureRuns runs;
    runs.end = capture.points.size();
    std::optional<std::pair<std::uint32_t, bool>> last;
    for (std::size_t p = capture.first; p < capture.points.size(); ++p) {
        const ChannelBits bits = capture.points[p];
        std::uint32_t value = 0;
        bool unknown = false;
        for (std::size_t b = 0; b < channels.size(); ++b) {
            unknown = unknown || (bits.unknown >> channels[b] & 1) != 0;
            value |= (bits.high >> channels[b] & 1) << b;
        }
        const std::pair now{unknown ? 0 : value, unknown};
        if (now != last) {
            runs.start.push_back(static_cast<std::uint32_t>(p));
            runs.value.push_back(now.first);
            runs.unknown.push_back(now.second ? 1 : 0);
            last = now;
        }
    }
    return runs;
}

const CaptureRuns& AnalyzerWindow::runs(const std::vector<std::size_t>& channels) {
    const auto it = runs_.find(channels);
    if (it != runs_.end()) {
        return it->second;
    }
    return runs_.emplace(channels, RunsOf(*shown_, channels)).first->second;
}

std::vector<AnalyzerWindow::Row> AnalyzerWindow::rows(const Session& session) const {
    // Groups and loose Channels, ordered by their first Channel.
    std::vector<std::pair<std::size_t, Row>> top;
    for (std::size_t g = 0; g < session.groups().size(); ++g) {
        top.push_back({std::ranges::min(session.groups()[g].channels), Row{.group = g}});
    }
    for (std::size_t c = 0; c < kChannels; ++c) {
        if (session.clips()[c] && !session.groupOf(c)) {
            top.push_back({c, Row{.channel = c}});
        }
    }
    std::ranges::sort(top, {}, &std::pair<std::size_t, Row>::first);
    std::vector<Row> rows;
    for (const auto& [first, row] : top) {
        rows.push_back(row);
        if (row.group && expanded_.contains(session.groups()[*row.group].name)) {
            const auto& channels = session.groups()[*row.group].channels;
            for (auto c = channels.rbegin(); c != channels.rend(); ++c) {
                rows.push_back(Row{.group = row.group, .channel = *c, .member = true});
            }
        }
    }
    return rows;
}

void AnalyzerWindow::draw(bool& open, Session& session, BoardView& board) {
    if (session.captures().update()) {
        shown_ = session.captures().front();
        runs_.clear();
        marker_.reset();
        fit();
    }
    if (!open) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize({860, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + 20, viewport->WorkPos.y + viewport->WorkSize.y - 440},
        ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Logic Analyzer", &open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    // Header.
    if (ToolButton("Clip (C)", board.tool() == Tool::Clip)) {
        board.setTool(Tool::Clip);
    }
    ImGui::SameLine(0, 16);
    const AnalyzerPhase phase = session.analyzerPhase();
    const bool running = phase == AnalyzerPhase::Armed || phase == AnalyzerPhase::Filling;
    if (ImGui::Button(running ? "Stop" : "Arm", {60, 0})) {
        if (running) {
            session.disarm();
        } else {
            session.arm();
        }
    }
    ImGui::SameLine(0, 16);
    ImGui::TextUnformatted(PhaseText(phase, session.powered()));
    AnalyzerSettings settings = session.analyzerSettings();
    bool changed = false;
    ImGui::SameLine(300);
    ImGui::SetNextItemWidth(90);
    if (ImGui::BeginCombo("Rate", FormatFrequency(settings.period).c_str())) {
        for (const Time period : kAnalyzerPeriods) {
            if (ImGui::Selectable(FormatFrequency(period).c_str(), period == settings.period)) {
                settings.period = period;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(70);
    if (ImGui::BeginCombo("Trigger at", kPositions[static_cast<std::size_t>(settings.position)])) {
        for (std::size_t p = 0; p < kPositions.size(); ++p) {
            if (ImGui::Selectable(kPositions[p],
                                  p == static_cast<std::size_t>(settings.position))) {
                settings.position = static_cast<TriggerPosition>(p);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 16);
    ImGui::BeginDisabled(!shown_);
    if (ImGui::Button("Fit")) {
        fit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Trigger") && shown_) {
        const double span = to_ - from_;
        from_ = static_cast<double>(shown_->trigger) - span / 2;
        to_ = from_ + span;
    }
    ImGui::EndDisabled();
    std::erase_if(selected_, [&](std::size_t c) { return !session.clips()[c].has_value(); });
    if (selected_.size() >= 2) {
        ImGui::SameLine(0, 16);
        if (ImGui::Button("Group")) {
            // Selected in row order, the top row most significant.
            std::vector<std::size_t> picked;
            for (const Row& row : rows(session)) {
                if ((!row.group || row.member) && selected_.contains(row.channel)) {
                    picked.push_back(row.channel);
                }
            }
            session.group(picked);
            selected_.clear();
        }
    }
    const auto clipped = static_cast<std::size_t>(
        std::ranges::count_if(session.clips(), [](const auto& c) { return c.has_value(); }));
    ImGui::SameLine(0, 16);
    ImGui::TextColored(kGrey, "%zu of %zu clipped", clipped, kChannels);

    // Rows: labels left, one canvas of lanes right; scrolls when they do not fit.
    const std::vector<Row> list = rows(session);
    const float footer = kAxisHeight + ImGui::GetTextLineHeightWithSpacing() + 4;
    ImGui::BeginChild("rows", {0, -footer});
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float laneX0 = origin.x + kLabelWidth;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x - kLabelWidth);
    const float laneX1 = laneX0 + width;
    const float lanesHeight = std::max(kLaneHeight, kLaneHeight * static_cast<float>(list.size()));
    const ImVec2 visibleMin = ImGui::GetWindowPos();
    const float visibleTop = std::max(origin.y, visibleMin.y);
    const float visibleBottom =
        std::min(origin.y + lanesHeight, visibleMin.y + ImGui::GetWindowHeight());

    // Canvas input, once, before anything is drawn: every lane uses the same view.
    ImGui::SetCursorScreenPos({laneX0, origin.y});
    ImGui::InvisibleButton("##canvas", {width, lanesHeight});
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const float mouseX = ImGui::GetIO().MousePos.x;
    std::optional<float> hoverX;
    if (shown_) {
        const double span = to_ - from_;
        const double perPixel = span / static_cast<double>(width);
        if (ImGui::IsItemHovered()) {
            hoverX = mouseX;
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0) {
                const double at = from_ + (mouseX - laneX0) * perPixel;
                const double next =
                    std::clamp(span * std::pow(kZoomStep, wheel), kMinPoints, double{kDepth});
                from_ = at - (mouseX - laneX0) / width * next;
                to_ = from_ + next;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                fit();
            }
        }
        if (ImGui::IsItemActivated()) {
            pressX_ = mouseX;
            moved_ = false;
        }
        if (ImGui::IsItemActive()) {
            moved_ = moved_ || std::abs(mouseX - pressX_) > 3;
            const double delta = ImGui::GetIO().MouseDelta.x * (to_ - from_) / width;
            from_ -= delta;
            to_ -= delta;
        }
        if (ImGui::IsItemDeactivated() && !moved_) {
            marker_ = from_ + (mouseX - laneX0) * (to_ - from_) / width;
        }
        const double shift = std::clamp(from_, 0.0, double{kDepth} - (to_ - from_)) - from_;
        from_ += shift;
        to_ += shift;
    }
    const double span = to_ - from_;
    const auto x = [&](double point) {
        return static_cast<float>(laneX0 + (point - from_) / span * width);
    };
    const auto pointAt = [&](float px) { return from_ + (px - laneX0) / width * span; };
    // The point whose values the labels show: under the cursor, else the marker.
    const std::optional<double> readPoint = hoverX ? std::optional(pointAt(*hoverX)) : marker_;

    // One lane: runs of a value, drawn as a level (one Channel) or a bus (several).
    const auto lane = [&](const CaptureRuns& runs, float top, ImU32 colour, bool bus,
                          std::size_t bits) {
        if (runs.start.empty()) {
            return;
        }
        const float high = top + 3, low = top + kLaneHeight - 3, mid = top + kLaneHeight / 2;
        draw.PushClipRect({laneX0, top}, {laneX1, top + kLaneHeight}, true);
        std::size_t r = runs.at(std::max(from_, static_cast<double>(runs.start.front())));
        std::optional<std::pair<float, float>> busy;  // merged sub-pixel runs
        const auto flush = [&] {
            if (busy) {
                draw.AddRectFilled({busy->first, high}, {busy->second, low}, colour);
                busy.reset();
            }
        };
        std::optional<int> previous;  // level of the last drawn run: 0, 1, X = 2
        while (r < runs.start.size() && runs.start[r] < to_) {
            const double end =
                r + 1 < runs.start.size() ? runs.start[r + 1] : static_cast<double>(runs.end);
            const float x0 = x(std::max<double>(runs.start[r], from_));
            const float x1 = x(std::min(end, to_));
            if (x1 - x0 < 1.0f) {
                // Sub-pixel: the pixel is busy; jump to the run at the next pixel.
                const float px = std::floor(x0);
                if (busy && px <= busy->second) {
                    busy->second = px + 1;
                } else {
                    flush();
                    busy = std::pair{px, px + 1};
                }
                const std::size_t next = runs.at(pointAt(px + 1));
                r = std::max(r + 1, next);
                previous.reset();
                continue;
            }
            flush();
            const bool unknown = runs.unknown[r] != 0;
            if (bus) {
                const float slant = std::min(3.0f, (x1 - x0) / 2);
                const ImU32 edge = unknown ? kUnknown : colour;
                const ImVec2 shape[6] = {{x0, mid}, {x0 + slant, high}, {x1 - slant, high},
                                         {x1, mid}, {x1 - slant, low},  {x0 + slant, low}};
                if (unknown) {
                    draw.AddConvexPolyFilled(shape, 6, kUnknown);
                } else {
                    draw.AddPolyline(shape, 6, edge, ImDrawFlags_Closed, 1.5f);
                }
                const std::string text = BusText(runs.value[r], unknown, bits);
                const ImVec2 size = ImGui::CalcTextSize(text.c_str());
                if (size.x + 2 * slant + 4 < x1 - x0) {
                    const float left = std::clamp((x0 + x1 - size.x) / 2, laneX0 + slant,
                                                  std::max(laneX0 + slant, x1 - size.x - slant));
                    draw.AddText({left, mid - size.y / 2}, unknown ? kBusText : colour,
                                 text.c_str());
                }
            } else if (unknown) {
                draw.AddRectFilled({x0, high}, {x1, low}, kUnknown);
                previous = 2;
            } else {
                const int level = static_cast<int>(runs.value[r]);
                const float y = level == 1 ? high : low;
                draw.AddLine({x0, y}, {x1, y}, colour, 1.5f);
                if (previous == 1 - level) {
                    draw.AddLine({x0, high}, {x0, low}, colour, 1.5f);
                }
                previous = level;
            }
            ++r;
        }
        flush();
        draw.PopClipRect();
    };

    // Rows.
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Row& row = list[i];
        const float top = origin.y + kLaneHeight * static_cast<float>(i);
        if (top + kLaneHeight < visibleTop || top > visibleBottom) {
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const ChannelGroup* group = row.group ? &session.groups()[*row.group] : nullptr;
        const std::vector<std::size_t> channels =
            group && !row.member ? group->channels : std::vector{row.channel};
        const ImU32 colour = group ? GroupColour(*row.group) : ChannelColour(row.channel);
        if (i % 2 == 1) {
            draw.AddRectFilled({origin.x, top}, {laneX1, top + kLaneHeight},
                               IM_COL32(255, 255, 255, 8));
        }
        ImGui::SetCursorScreenPos({origin.x + (row.member ? 16.0f : 0.0f), top + 1});
        ImGui::ColorButton("##colour", ImColor(colour),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {12, 12});
        ImGui::SameLine();
        std::string value;
        if (group && !row.member) {
            const bool unfolded = expanded_.contains(group->name);
            if (ImGui::ArrowButton("##expand", unfolded ? ImGuiDir_Down : ImGuiDir_Right)) {
                if (unfolded) {
                    expanded_.erase(group->name);
                } else {
                    expanded_.insert(group->name);
                }
            }
            ImGui::SameLine();
            ImGui::Text("%s  %zu", group->name.c_str(), group->channels.size());
            if (shown_ && readPoint) {
                const CaptureRuns& r = runs(channels);
                if (!r.start.empty() && *readPoint >= r.start.front()) {
                    const std::size_t at = r.at(*readPoint);
                    value = BusText(r.value[at], r.unknown[at] != 0, channels.size());
                }
            }
        } else {
            const auto& clip = session.clips()[row.channel];
            const std::string label = clip ? clip->label : "free";
            if (ImGui::Selectable(label.c_str(), selected_.contains(row.channel), 0, {120, 0})) {
                if (!selected_.erase(row.channel)) {
                    selected_.insert(row.channel);
                }
            }
            ImGui::SetItemTooltip("click: select for a group");
            if (shown_ && readPoint) {
                const CaptureRuns& r = runs(channels);
                if (!r.start.empty() && *readPoint >= r.start.front()) {
                    const std::size_t at = r.at(*readPoint);
                    value = r.unknown[at] != 0 ? "X" : std::to_string(r.value[at]);
                }
            }
        }
        ImGui::SameLine(origin.x - ImGui::GetWindowPos().x + 190);
        ImGui::TextUnformatted(value.c_str());
        ImGui::SameLine(origin.x - ImGui::GetWindowPos().x + 240);
        ImGui::SetNextItemWidth(60);
        if (group && !row.member) {
            ImGui::SetNextItemWidth(
                std::max(60.0f, ImGui::CalcTextSize("0").x *
                                        static_cast<float>((group->channels.size() + 3) / 4) +
                                    12));
            PatternEdit& edit = patterns_[group->name];
            if (editing_ != group->name) {
                const std::string text = session.groupTrigger(*row.group);
                std::ranges::fill(edit.text, '\0');
                std::ranges::copy(text.substr(0, edit.text.size() - 1), edit.text.begin());
            }
            if (edit.refused) {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, kRefused);
            }
            ImGui::InputText("##pattern", edit.text.data(), edit.text.size());
            if (edit.refused) {
                ImGui::PopStyleColor();
            }
            ImGui::SetItemTooltip("bus Trigger: hex, x = any (E0xx)");
            if (ImGui::IsItemActive()) {
                editing_ = group->name;
            } else if (editing_ == group->name) {
                editing_.clear();
            }
            if (ImGui::IsItemEdited()) {
                edit.refused = false;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                edit.refused = !session.setGroupTrigger(*row.group, edit.text.data());
                settings = session.analyzerSettings();
            }
        } else if (row.member) {
            ImGui::TextColored(
                kGrey, "%s", kConditions[static_cast<std::size_t>(settings.trigger[row.channel])]);
        } else {
            const auto condition = static_cast<std::size_t>(settings.trigger[row.channel]);
            if (ImGui::BeginCombo("##trigger", kConditions[condition])) {
                for (std::size_t t = 0; t < kConditions.size(); ++t) {
                    if (ImGui::Selectable(kConditions[t], t == condition)) {
                        settings.trigger[row.channel] = static_cast<TriggerCondition>(t);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::SameLine(origin.x - ImGui::GetWindowPos().x + 316);
        if (ImGui::SmallButton("×")) {
            if (group && !row.member) {
                if (ImGui::GetIO().KeyShift) {
                    session.unclipGroup(*row.group);
                } else {
                    session.ungroup(*row.group);
                }
            } else {
                session.unclip(row.channel);
            }
            ImGui::PopID();
            break;  // rows changed under us
        }
        ImGui::SetItemTooltip(group && !row.member ? "ungroup; shift: unclip the group" : "unclip");

        // Lane.
        if (shown_) {
            const bool bus = channels.size() > 1;
            lane(runs(channels), top, colour, bus, channels.size());
            if (!bus && !row.member) {
                const auto& clip = session.clips()[row.channel];
                const std::string& was = shown_->labels[row.channel];
                if (!clip || clip->label != was) {
                    draw.AddText({laneX0 + 4, top + 3}, kFaint,
                                 was.empty() ? "not clipped in this capture" : was.c_str());
                }
            }
        } else if (i == 0) {
            draw.AddText({laneX0 + 4, top + 3}, ImGui::GetColorU32(kGrey), "arm, then power on");
        }
        ImGui::PopID();
    }
    if (list.empty()) {
        draw.AddText({origin.x + 4, origin.y + 3}, ImGui::GetColorU32(kGrey), kEmptyHint);
    }

    // Trigger, marker, cursor over the visible lanes.
    if (shown_) {
        draw.PushClipRect({laneX0, visibleTop}, {laneX1, visibleBottom}, true);
        const float trigger = x(static_cast<double>(shown_->trigger));
        draw.AddLine({trigger, visibleTop}, {trigger, visibleBottom}, kTriggerLine, 1.0f);
        if (marker_) {
            draw.AddLine({x(*marker_), visibleTop}, {x(*marker_), visibleBottom}, kMarkerLine,
                         1.0f);
        }
        if (hoverX) {
            draw.AddLine({*hoverX, visibleTop}, {*hoverX, visibleBottom}, kCursorLine, 1.0f);
        }
        draw.PopClipRect();
    }
    ImGui::EndChild();

    // Axis and readout.
    if (shown_) {
        ImDrawList& list2 = *ImGui::GetWindowDrawList();
        const ImVec2 axis = ImGui::GetCursorScreenPos();
        const auto timeAt = [&](double point) {
            return static_cast<Time>(std::llround((point - static_cast<double>(shown_->trigger)) *
                                                  static_cast<double>(shown_->period)));
        };
        list2.PushClipRect({laneX0, axis.y}, {laneX1, axis.y + kAxisHeight}, true);
        const double spanTime = span * static_cast<double>(shown_->period);
        const Time step = TickStep(spanTime, std::max(2, static_cast<int>(width / 90)));
        for (Time t = (timeAt(from_) / step - 1) * step; t <= timeAt(to_); t += step) {
            const double point = static_cast<double>(shown_->trigger) +
                                 static_cast<double>(t) / static_cast<double>(shown_->period);
            const float tx = x(point);
            if (tx < laneX0 || tx > laneX1) {
                continue;
            }
            list2.AddLine({tx, axis.y}, {tx, axis.y + 4}, kAxis);
            list2.AddText({tx + 2, axis.y + 2}, kAxis, FormatTime(t, true).c_str());
        }
        list2.PopClipRect();
        ImGui::Dummy({0, kAxisHeight});
        if (hoverX) {
            const double cursor = pointAt(*hoverX);
            std::string readout = "t = " + FormatTime(timeAt(cursor), true);
            if (marker_) {
                readout += "   dt = " + FormatTime(timeAt(cursor) - timeAt(*marker_), true);
            }
            ImGui::TextUnformatted(readout.c_str());
        } else {
            ImGui::TextColored(kGrey,
                               "wheel: zoom   drag: pan   click: marker   double-click: fit   "
                               "shift-click a pin: whole bus");
        }
    }
    if (changed) {
        session.setAnalyzer(settings);
    }
    ImGui::End();
}

}  // namespace bw
