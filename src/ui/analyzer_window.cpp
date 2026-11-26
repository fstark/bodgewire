#include "ui/analyzer_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <string>
#include <utility>

#include <imgui.h>

#include "ui/bench.hpp"
#include "ui/board_view.hpp"
#include "ui/controls.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/touch.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

// A row and its lane, as high as a control (P8f); the overview strip as high.
constexpr float kLaneHeight = kControl;
constexpr float kOverviewHeight = 28.0f;
constexpr float kLabelWidth = 360.0f;
constexpr float kValueX = 190.0f, kTriggerX = 240.0f, kTriggerWidth = 76.0f, kUnclipX = 324.0f;
constexpr float kEdgeReach = 12;  // px within which a finger's cursor lands on an edge
constexpr ImU32 kUnknown = IM_COL32(140, 140, 140, 110);
constexpr ImU32 kGridLine = IM_COL32(255, 255, 255, 22);
constexpr ImU32 kOutside = IM_COL32(0, 0, 0, 70);  // beyond the Capture
constexpr ImU32 kOverviewBar = IM_COL32(20, 21, 24, 255);
constexpr ImU32 kOverviewBox = IM_COL32(230, 230, 230, 255);
constexpr ImU32 kFaint = IM_COL32(200, 200, 200, 90);
constexpr ImU32 kBusText = IM_COL32(20, 20, 20, 255);
constexpr std::array<const char*, 5> kConditions{"any", "0", "1", "rise", "fall"};
constexpr std::array<TriggerPosition, 3> kPositions{TriggerPosition::Early, TriggerPosition::Middle,
                                                    TriggerPosition::Late};
constexpr const char* kEmptyHint =
    "Clip, then click a chip pin. The pin / bus switch clips a whole bus.";
constexpr const char* kGestures =
    "wheel or pinch: zoom   drag: pan or scroll   click: marker   hold and slide: cursor   "
    "double-click: fit";

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

// `width` bits in hex, X for a value with an unknown member.
std::string BusText(std::uint32_t value, bool unknown, std::size_t width) {
    const std::size_t digits = (width + 3) / 4;
    if (unknown) {
        return std::string(digits, 'X');
    }
    return std::format("{:0{}X}", value, digits);
}

Time BeforeTrigger(TriggerPosition position, Time period) {
    return static_cast<Time>(PreTriggerPoints(position)) * period;
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
            value |= static_cast<std::uint32_t>(bits.high >> channels[b] & 1) << b;  // b < 32
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

TimeRange RecordOf(const Capture& capture) {
    const auto trigger = static_cast<Time>(capture.trigger);
    const auto first = static_cast<Time>(capture.first);
    const auto points = static_cast<Time>(capture.points.size());
    return {.from = (first - trigger) * capture.period, .to = (points - trigger) * capture.period};
}

const CaptureRuns& AnalyzerWindow::runs(const std::vector<std::size_t>& channels) {
    const auto it = runs_.find(channels);
    if (it != runs_.end()) {
        return it->second;
    }
    return runs_.emplace(channels, RunsOf(*shown_, channels)).first->second;
}

void AnalyzerWindow::preset(std::optional<TimeView> view, std::optional<Time> marker,
                            std::optional<Time> cursor) {
    presetView_ = view;
    presetMarker_ = marker;
    placedCursor_ = cursor;
}

void AnalyzerWindow::zoom(Time anchor, int notches) {
    const TimeRange record = RecordOf(*shown_);
    view_ = ZoomAbout(view_, anchor, notches, StepAtLeast(shown_->period), Fit(record).perDivision);
}

std::vector<AnalyzerWindow::Row> AnalyzerWindow::rows(const Session& session) const {
    // The setup's rows in order (P8i), a group's members under it when expanded.
    const std::vector<AnalyzerRow> top = session.analyzer().rows();
    std::vector<Row> rows;
    for (std::size_t place = 0; place < top.size(); ++place) {
        const AnalyzerRow& row = top[place];
        rows.push_back(Row{.group = row.group, .channel = row.channel, .place = place});
        if (row.group && expanded_.contains(session.analyzer().groups()[*row.group].name)) {
            const auto& channels = session.analyzer().groups()[*row.group].channels;
            for (auto c = channels.rbegin(); c != channels.rend(); ++c) {
                rows.push_back(
                    Row{.group = row.group, .channel = *c, .member = true, .place = place});
            }
        }
    }
    return rows;
}

bool AnalyzerWindow::draw(bool& open, Session& session) {
    if (session.analyzer().captures().update()) {
        shown_ = session.analyzer().captures().front();
        runs_.clear();
        marker_.reset();
        const TimeRange record = RecordOf(*shown_);
        if (presetView_) {
            // Its step within the zoom limits, about its centre.
            const Time centre = presetView_->left + presetView_->span() / 2;
            const Time step = std::clamp(presetView_->perDivision, StepAtLeast(shown_->period),
                                         Fit(record).perDivision);
            view_ = Clamp(CentredOn(step, centre), record);
            presetView_.reset();
        } else if (shownPeriod_ == shown_->period) {
            view_ = Clamp(view_, record);  // the same view on the new Capture
        } else {
            view_ = FirstView(record, shown_->period);
        }
        shownPeriod_ = shown_->period;
        marker_ = std::exchange(presetMarker_, std::nullopt);
    }
    if (!open) {
        return false;
    }
    if (!BeginInstrument(kAnalyzerWindowName, open, BenchObject::Analyzer)) {
        ImGui::End();
        return false;
    }
    const bool clicked = InstrumentClicked();
    const float padX = ImGui::GetStyle().WindowPadding.x;
    const float contentX = ImGui::GetCursorScreenPos().x;
    const float laneX0 = contentX + kLabelWidth;
    // The rows child may show a scrollbar: every strip keeps its width free, so they all line up.
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x - kLabelWidth -
                                           ImGui::GetStyle().ScrollbarSize);
    const float laneX1 = laneX0 + width;

    // Header line 1 (P8f: a stepper and choices, no dropdowns): arm, the phase, the sampling,
    // where the Trigger sits in the Capture.
    const AnalyzerPhase phase = session.analyzer().phase();
    if (ImGui::Button("Arm", {60, kBig})) {
        session.analyzer().arm();
    }
    ImGui::SameLine(0, 12);
    // A slot as wide as the longest phase, so the settings after it never move.
    const float phaseX = ImGui::GetCursorPosX();
    BigText(PhaseText(phase, session.powered()));
    AnalyzerSettings settings = session.analyzer().settings();
    bool changed = false;
    ImGui::SameLine(phaseX + ImGui::CalcTextSize("armed: waiting for trigger").x + 16);
    BigText("Sample every");
    ImGui::SameLine();
    // The periods run from the longest to the shortest: + samples finer.
    static const std::vector<std::string> kPeriodNames = [] {
        std::vector<std::string> names;
        for (const Time period : kAnalyzerPeriods) {
            names.push_back(FormatTime(period, false));
        }
        return names;
    }();
    auto periodStep = static_cast<std::size_t>(
        std::ranges::find(kAnalyzerPeriods, settings.period) - kAnalyzerPeriods.begin());
    periodStep = std::min(periodStep, kAnalyzerPeriods.size() - 1);
    if (Stepper("##period", kPeriodNames, periodStep, 84)) {
        settings.period = kAnalyzerPeriods[periodStep];
        changed = true;
    }
    ImGui::SameLine(0, 16);
    BigText("Before T");
    ImGui::SameLine();
    std::array<std::string, kPositions.size()> beforeTexts;
    std::array<const char*, kPositions.size()> beforeLabels{};
    for (std::size_t i = 0; i < kPositions.size(); ++i) {
        beforeTexts[i] = FormatTime(BeforeTrigger(kPositions[i], settings.period), false);
        beforeLabels[i] = beforeTexts[i].c_str();
    }
    auto position = static_cast<std::size_t>(std::ranges::find(kPositions, settings.position) -
                                             kPositions.begin());
    if (Choice("##position", beforeLabels, position, kBig)) {
        settings.position = kPositions[position];
        changed = true;
    }

    // Header line 2: Channels left, the view's controls over the lanes.
    const auto clipped = static_cast<std::size_t>(std::ranges::count_if(
        session.analyzer().clips(), [](const auto& c) { return c.has_value(); }));
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kHintGrey, "%zu of %zu clipped", clipped, kChannels);
    // The pin / bus switch (P8f): what Shift does to a click on a pin.
    ImGui::SameLine(0, 12);
    constexpr std::array<const char*, 2> kClipModes{"pin", "bus"};
    std::size_t clipMode = bus_ ? 1 : 0;
    if (Choice("##clip", kClipModes, clipMode)) {
        bus_ = clipMode == 1;
    }
    ImGui::SetItemTooltip("bus: a click on a pin clips, or unclips, its whole bus");
    std::erase_if(selected_,
                  [&](std::size_t c) { return !session.analyzer().clips()[c].has_value(); });
    if (selected_.size() >= 2) {
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Group")) {
            // Selected in row order, the top row most significant.
            std::vector<std::size_t> picked;
            for (const Row& row : rows(session)) {
                if ((!row.group || row.member) && selected_.contains(row.channel)) {
                    picked.push_back(row.channel);
                }
            }
            session.analyzer().group(picked);
            selected_.clear();
        }
    }
    ImGui::SameLine(padX + kLabelWidth);
    ImGui::BeginDisabled(!shown_);
    if (ImGui::Button("-", {kBig, 0}) && shown_) {
        zoom(view_.left + view_.span() / 2, -1);
    }
    ImGui::SetItemTooltip("zoom out (wheel or pinch)");
    ImGui::SameLine(0, 1);
    const std::string perDivision = FormatTime(view_.perDivision, false) + "/div";
    ImGui::Button(shown_ ? perDivision.c_str() : "-/div", {100, 0});
    ImGui::SameLine(0, 1);
    if (ImGui::Button("+", {kBig, 0}) && shown_) {
        zoom(view_.left + view_.span() / 2, 1);
    }
    ImGui::SetItemTooltip("zoom in (wheel or pinch)");
    ImGui::SameLine(0, 12);
    if (ImGui::Button("Fit", {kBig, 0}) && shown_) {
        view_ = Fit(RecordOf(*shown_));
    }
    ImGui::SetItemTooltip("the whole Capture (double-click)");
    ImGui::SameLine(0, 4);
    if (ImGui::Button("T", {kBig, 0}) && shown_) {
        view_ = CentredOn(view_.perDivision, 0);
    }
    ImGui::SetItemTooltip("centre on the Trigger");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 16);
    ImGui::TextColored(kHintGrey, "%s captured",
                       FormatTime(static_cast<Time>(kDepth) * settings.period, false).c_str());

    // Both read view_ as it is when called: input below changes it before anything is drawn.
    const auto x = [&](Time t) {
        return laneX0 + static_cast<float>(static_cast<double>(t - view_.left) /
                                           static_cast<double>(view_.span()) * width);
    };
    const auto timeAt = [&](float px) {
        return view_.left + static_cast<Time>(std::llround((px - laneX0) / width *
                                                           static_cast<double>(view_.span())));
    };
    const float mouseX = ImGui::GetIO().MousePos.x;

    // Overview and axis strip: their rows reserved and the overview's press taken here; both are
    // drawn after the canvas input, from the view this frame settles.
    const float overviewTop = ImGui::GetCursorScreenPos().y + 2;
    ImGui::SetCursorScreenPos({laneX0, overviewTop});
    ImGui::InvisibleButton("##overview", {width, kOverviewHeight});
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    if (shown_) {
        const TimeRange record = RecordOf(*shown_);
        if (ImGui::IsItemActive()) {
            const Time centre =
                record.from + static_cast<Time>(std::llround((mouseX - laneX0) / width *
                                                             static_cast<double>(record.length())));
            view_ = Clamp(CentredOn(view_.perDivision, centre), record);
        }
        if (ImGui::IsItemHovered()) {
            const int notches = WheelNotches(ImGui::GetIO().MouseWheel, wheelRest_);
            if (notches != 0) {
                zoom(view_.left + view_.span() / 2, notches);
                view_ = Clamp(view_, record);
            }
        }
    }
    ImGui::SetCursorScreenPos({contentX, overviewTop + kOverviewHeight + 2});
    const float axisY = ImGui::GetCursorScreenPos().y;
    ImGui::Dummy({0, kAxisHeight});

    // Rows: labels left, one canvas of lanes right; scrolls when they do not fit.
    const std::vector<Row> list = rows(session);
    const float footer = ImGui::GetTextLineHeightWithSpacing() + 4;
    // While the rows do not fit, a drag on their labels scrolls them (P8f) instead of moving
    // the window.
    ImGui::BeginChild("rows", {0, -footer}, ImGuiChildFlags_None,
                      rowsScroll_ ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None);
    rowsScroll_ = ImGui::GetScrollMaxY() > 0;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float lanesHeight = std::max(kLaneHeight, kLaneHeight * static_cast<float>(list.size()));
    const ImVec2 visibleMin = ImGui::GetWindowPos();
    const float visibleTop = std::max(origin.y, visibleMin.y);
    const float visibleBottom =
        std::min(origin.y + lanesHeight, visibleMin.y + ImGui::GetWindowHeight());

    // Canvas input, once, before anything is drawn: every lane uses the same view.
    ImGui::SetCursorScreenPos({laneX0, origin.y});
    ImGui::InvisibleButton("##canvas", {width, lanesHeight});
    const TimeCanvasInput input = canvas_.read(
        width, kDivisions, shown_ && marker_ ? std::optional(x(*marker_)) : std::nullopt,
        visibleTop, rowsScroll_);
    const bool canvasHeld = ImGui::IsItemActive();
    if (input.scrollY != 0) {
        ImGui::SetScrollY(ImGui::GetScrollY() + input.scrollY);  // a vertical drag on the lanes
    }
    std::optional<Time> cursor;
    if (shown_) {
        const TimeRange record = RecordOf(*shown_);
        if (input.zoom != 0) {
            zoom(timeAt(*input.hover), input.zoom);
        }
        if (input.pan != 0) {
            view_ = Pan(view_, input.pan, width);
        }
        if (input.fit) {
            view_ = Fit(record);
        }
        if (input.clearMarker) {
            marker_.reset();
        }
        if (input.dragStep != 0) {
            view_ = Pan(view_, input.dragStep, width);
        }
        // A finger's Marker and cursor land on an edge of the row it is on, within 12 px (P8f).
        const auto landed = [&](float px) {
            const Time t = timeAt(px);
            if (!FingerPointer() || list.empty()) {
                return t;
            }
            const auto index = std::clamp<std::ptrdiff_t>(
                static_cast<std::ptrdiff_t>((input.pressY - origin.y) / kLaneHeight), 0,
                static_cast<std::ptrdiff_t>(list.size()) - 1);
            const Row& on = list[static_cast<std::size_t>(index)];
            const std::vector<std::size_t> channels =
                on.group && !on.member ? session.analyzer().groups()[*on.group].channels
                                       : std::vector{on.channel};
            const CaptureRuns& r = runs(channels);
            if (r.start.size() < 2) {
                return t;
            }
            const double samplePeriod = static_cast<double>(shown_->period);
            const double triggerPoint = static_cast<double>(shown_->trigger);
            const double reach =
                kEdgeReach / width * static_cast<double>(view_.span()) / samplePeriod;
            const auto edge =
                SnapToEdge(std::span(r.start).subspan(1),  // the first start is no edge
                           triggerPoint + static_cast<double>(t) / samplePeriod, reach);
            return edge ? static_cast<Time>(std::llround((*edge - triggerPoint) * samplePeriod))
                        : t;
        };
        if (input.marker) {
            marker_ = landed(*input.marker);
        }
        if (input.scrub) {
            placedCursor_ = landed(*input.scrub);
        }
        view_ = Clamp(view_, record);
        if (input.hover) {
            cursor = timeAt(*input.hover);
        } else if (placedCursor_) {
            cursor = placedCursor_;
        }
    }
    const double period = shown_ ? static_cast<double>(shown_->period) : 1.0;
    const double trigger = shown_ ? static_cast<double>(shown_->trigger) : 0.0;
    const auto pointOf = [&](Time t) { return trigger + static_cast<double>(t) / period; };
    const auto pointX = [&](double point) {
        return x(static_cast<Time>(std::llround((point - trigger) * period)));
    };
    const double fromPoint = pointOf(view_.left);
    const double toPoint = pointOf(view_.left + view_.span());
    // The point whose values the labels show: under the cursor, else the marker.
    const std::optional<Time> readTime = cursor ? cursor : marker_;

    // Behind the lanes: the grid, darker beyond the Capture.
    if (shown_) {
        draw.PushClipRect({laneX0, visibleTop}, {laneX1, visibleBottom}, true);
        DrawGrid(draw, LabelledGrid(view_, width), x, visibleTop, visibleBottom, kGridLine);
        const TimeRange record = RecordOf(*shown_);
        if (x(record.from) > laneX0) {
            draw.AddRectFilled({laneX0, visibleTop}, {x(record.from), visibleBottom}, kOutside);
        }
        if (x(record.to) < laneX1) {
            draw.AddRectFilled({x(record.to), visibleTop}, {laneX1, visibleBottom}, kOutside);
        }
        draw.PopClipRect();
    }

    // One lane: runs of a value, drawn as a level (one Channel) or a bus (several).
    const auto lane = [&](const CaptureRuns& runs, float top, ImU32 colour, bool bus,
                          std::size_t bits) {
        if (runs.start.empty()) {
            return;
        }
        const float high = top + 3, low = top + kLaneHeight - 3, mid = top + kLaneHeight / 2;
        draw.PushClipRect({laneX0, top}, {laneX1, top + kLaneHeight}, true);
        std::size_t r = runs.at(std::max(fromPoint, static_cast<double>(runs.start.front())));
        std::optional<std::pair<float, float>> busy;  // merged sub-pixel runs
        const auto flush = [&] {
            if (busy) {
                draw.AddRectFilled({busy->first, high}, {busy->second, low}, colour);
                busy.reset();
            }
        };
        std::optional<int> previous;  // level of the last drawn run: 0, 1, X = 2
        while (r < runs.start.size() && runs.start[r] < toPoint) {
            const double end =
                r + 1 < runs.start.size() ? runs.start[r + 1] : static_cast<double>(runs.end);
            // True ends, kept within a pixel past the lanes so a cut end has no slant inside.
            const float x0 = std::max(pointX(runs.start[r]), laneX0 - 1);
            const float x1 = std::min(pointX(end), laneX1 + 1);
            if (x1 - x0 < 1.0f) {
                // Sub-pixel: the pixel is busy; jump to the run at the next pixel.
                const float px = std::floor(x0);
                if (busy && px <= busy->second) {
                    busy->second = px + 1;
                } else {
                    flush();
                    busy = std::pair{px, px + 1};
                }
                const std::size_t next = runs.at(pointOf(timeAt(px + 1)));
                r = std::max(r + 1, next);
                previous.reset();
                continue;
            }
            flush();
            const bool unknown = runs.unknown[r] != 0;
            if (bus) {
                const bool cutLeft = x0<laneX0, cutRight = x1> laneX1;
                const float slant = std::min(3.0f, (x1 - x0) / 2);
                const float left = cutLeft ? 0.0f : slant, right = cutRight ? 0.0f : slant;
                const ImVec2 shape[6] = {{x0, mid}, {x0 + left, high}, {x1 - right, high},
                                         {x1, mid}, {x1 - right, low}, {x0 + left, low}};
                if (unknown) {
                    draw.AddConvexPolyFilled(shape, 6, kUnknown);
                } else {
                    draw.AddPolyline(shape, 6, colour, ImDrawFlags_Closed, 1.5f);
                }
                const std::string text = BusText(runs.value[r], unknown, bits);
                const ImVec2 size = ImGui::CalcTextSize(text.c_str());
                if (const auto textX = RunTextX(x0, x1, laneX0, laneX1, size.x, slant)) {
                    draw.AddText({*textX, mid - size.y / 2}, unknown ? kBusText : colour,
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
        const ChannelGroup* group = row.group ? &session.analyzer().groups()[*row.group] : nullptr;
        const std::vector<std::size_t> channels =
            group && !row.member ? group->channels : std::vector{row.channel};
        const ImU32 colour = RowColour(row.place);
        if (i % 2 == 1) {
            draw.AddRectFilled({origin.x, top}, {laneX1, top + kLaneHeight},
                               IM_COL32(255, 255, 255, 8));
        }
        // The row's controls, each placed on its line (P8f): the swatch, the fold arrow or the
        // name, the value, the Trigger, the x.
        const float indent = row.member ? 16.0f : 0.0f;
        const float textY = top + (kLaneHeight - ImGui::GetTextLineHeight()) / 2;
        draw.AddRectFilled({origin.x + indent, top + (kLaneHeight - 12) / 2},
                           {origin.x + indent + 12, top + (kLaneHeight + 12) / 2}, colour);
        const float nameX = origin.x + indent + 20;
        std::string value;
        const auto readValue = [&](bool asBus) {
            if (!shown_ || !readTime) {
                return;
            }
            const CaptureRuns& r = runs(channels);
            const double point = pointOf(*readTime);
            if (r.start.empty() || point < r.start.front() || point >= static_cast<double>(r.end)) {
                return;
            }
            const std::size_t at = r.at(point);
            value = asBus ? BusText(r.value[at], r.unknown[at] != 0, channels.size())
                          : (r.unknown[at] != 0 ? "X" : std::to_string(r.value[at]));
        };
        if (group && !row.member) {
            const bool unfolded = expanded_.contains(group->name);
            ImGui::SetCursorScreenPos({nameX, top});
            if (ImGui::ArrowButton("##expand", unfolded ? ImGuiDir_Down : ImGuiDir_Right)) {
                if (unfolded) {
                    expanded_.erase(group->name);
                } else {
                    expanded_.insert(group->name);
                }
            }
            ImGui::SetCursorScreenPos({nameX + kLaneHeight + 6, textY});
            ImGui::Text("%s[%zu:0]", group->name.c_str(), group->channels.size() - 1);
            readValue(true);
        } else {
            // The whole name cell, as high as the row, selects the Channel for a group.
            const auto& clip = session.analyzer().clips()[row.channel];
            const std::string label = clip ? clip->label : "free";
            ImGui::SetCursorScreenPos({nameX, top});
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.0f, 0.5f});
            if (ImGui::Selectable(label.c_str(), selected_.contains(row.channel), 0,
                                  {origin.x + kValueX - 8 - nameX, kLaneHeight})) {
                if (!selected_.erase(row.channel)) {
                    selected_.insert(row.channel);
                }
            }
            ImGui::PopStyleVar();
            ImGui::SetItemTooltip("click: select for a group");
            readValue(false);
        }
        draw.AddText({origin.x + kValueX, textY}, ImGui::GetColorU32(ImGuiCol_Text), value.c_str());
        if (group && !row.member) {
            ImGui::SetCursorScreenPos({origin.x + kTriggerX, top});
            std::string pattern = session.analyzer().groupTrigger(*row.group);
            bool& refused = patternRefused_[group->name];
            if (NumberField("##pattern", pattern, PadKind::Pattern, kTriggerWidth, refused,
                            (group->channels.size() + 3) / 4)) {
                refused = !session.analyzer().setGroupTrigger(*row.group, pattern);
                settings = session.analyzer().settings();
            }
            ImGui::SetItemTooltip("bus Trigger: hex, x = any (E0xx)");
        } else if (row.member) {
            draw.AddText({origin.x + kTriggerX, textY}, ImGui::GetColorU32(kHintGrey),
                         kConditions[static_cast<std::size_t>(settings.trigger[row.channel])]);
        } else {
            ImGui::SetCursorScreenPos({origin.x + kTriggerX, top});
            auto condition = static_cast<std::size_t>(settings.trigger[row.channel]);
            if (ChoicePopup("##trigger", kConditions, condition, kTriggerWidth, kLaneHeight)) {
                settings.trigger[row.channel] = static_cast<TriggerCondition>(condition);
                changed = true;
            }
        }
        ImGui::SetCursorScreenPos({origin.x + kUnclipX, top});
        bool rowsChanged = false;
        if (ImGui::Button("\u00d7", {kLaneHeight, kLaneHeight})) {
            if (group && !row.member) {
                if (ImGui::GetIO().KeyShift) {
                    session.analyzer().unclipGroup(*row.group);
                    rowsChanged = true;
                } else {
                    ImGui::OpenPopup("##group");  // its two choices (P8f)
                }
            } else {
                session.analyzer().unclip(row.channel);
                rowsChanged = true;
            }
        }
        ImGui::SetItemTooltip(group && !row.member ? "ungroup, or unclip the whole group"
                                                   : "unclip");
        if (group && !row.member && ImGui::BeginPopup("##group")) {
            if (ImGui::Button("Ungroup", {120, kBig})) {
                session.analyzer().ungroup(*row.group);
                rowsChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Unclip all", {120, kBig})) {
                session.analyzer().unclipGroup(*row.group);
                rowsChanged = true;
            }
            if (rowsChanged) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (rowsChanged) {
            ImGui::PopID();
            break;  // rows changed under us
        }

        // Lane.
        if (shown_) {
            const bool bus = channels.size() > 1;
            lane(runs(channels), top, colour, bus, channels.size());
            if (!bus && !row.member) {
                const auto& clip = session.analyzer().clips()[row.channel];
                const std::string& was = shown_->labels[row.channel];
                if (!clip || clip->label != was) {
                    draw.AddText({laneX0 + 4, top + 3}, kFaint,
                                 was.empty() ? "not clipped in this capture" : was.c_str());
                }
            }
        } else if (i == 0) {
            draw.AddText({laneX0 + 4, top + 3}, ImGui::GetColorU32(kHintGrey),
                         "arm, then power on");
        }
        ImGui::PopID();
    }
    if (list.empty()) {
        draw.AddText({origin.x + 4, origin.y + 3}, ImGui::GetColorU32(kHintGrey), kEmptyHint);
    }

    // Trigger, marker, cursor over the visible lanes.
    if (shown_) {
        draw.PushClipRect({laneX0, visibleTop}, {laneX1, visibleBottom}, true);
        DrawTrigger(draw, std::round(x(0)), visibleTop, visibleBottom);
        if (marker_) {
            DrawMarker(draw, std::round(x(*marker_)), visibleTop, visibleBottom);
        }
        if (cursor) {
            draw.AddLine({x(*cursor), visibleTop}, {x(*cursor), visibleBottom}, kCursorLine);
        }
        draw.PopClipRect();
    }
    labelScroll_.update(canvasHeld);  // a drag on the labels; the lanes' is the canvas's
    // Two fingers on the rows zoom the time and scroll them: no pinch of the window there.
    const bool rowsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGui::EndChild();

    // Overview: the whole record, the view as a box, T and the marker as ticks.
    ImDrawList& window = *ImGui::GetWindowDrawList();
    {
        const float top = overviewTop, bottom = top + kOverviewHeight;
        window.AddText({contentX + 4, top - 1}, ImGui::GetColorU32(kHintGrey), "overview");
        window.AddRectFilled({laneX0, top}, {laneX1, bottom}, kOverviewBar, 2.0f);
        if (shown_) {
            const TimeRange record = RecordOf(*shown_);
            const auto ox = [&](Time t) {
                return laneX0 + static_cast<float>(static_cast<double>(t - record.from) /
                                                   static_cast<double>(record.length()) * width);
            };
            window.PushClipRect({laneX0, top - 2}, {laneX1, bottom + 2}, true);
            const float box0 = std::max(laneX0, ox(view_.left));
            const float box1 = std::min(laneX1, std::max(ox(view_.left + view_.span()), box0 + 8));
            window.AddLine({ox(0), top - 2}, {ox(0), bottom + 2}, kTriggerMark, 2.0f);
            if (marker_) {
                window.AddLine({ox(*marker_), top}, {ox(*marker_), bottom}, kMarkerLine, 2.0f);
            }
            window.AddRect({box0, top - 1}, {box1, bottom + 1}, kOverviewBox, 2.0f, 0, 1.5f);
            window.PopClipRect();
        }
    }
    // Axis strip: column headings left, times from the Trigger over the lanes.
    {
        const ImU32 heading = ImGui::GetColorU32(kHintGrey);
        window.AddText({contentX + 4, axisY + 1}, heading, "Name");
        window.AddText({contentX + kValueX, axisY + 1}, heading, "Value");
        window.AddText({contentX + kTriggerX, axisY + 1}, heading, "Trigger");
        if (shown_) {
            DrawAxisLabels(window, view_, LabelledGrid(view_, width), x, axisY, laneX0, laneX1);
            DrawOffScreenTrigger(window, view_, laneX0, laneX1, axisY);
        }
    }

    if (shown_) {
        StatusLine(cursor, marker_, kGestures);
    } else {
        ImGui::TextColored(kHintGrey, "%s", "the pin / bus switch: a pin's whole bus");
    }
    if (changed) {
        session.analyzer().setSettings(settings);
    }
    PopOnDoubleClick(BenchObject::Analyzer);
    CornerGrip();
    PinchResize(rowsHovered);
    ImGui::End();
    return clicked;
}

}  // namespace bw
