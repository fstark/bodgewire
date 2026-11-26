#include "ui/analyzer_window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <string>
#include <utility>

#include <imgui.h>

#include "ui/bench.hpp"
#include "ui/board_view.hpp"
#include "ui/case_keys.hpp"
#include "ui/controls.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/touch.hpp"
#include "view/analyzer_layout.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr float kEdgeReach = 12;  // px within which a cursor or Marker lands on an edge
// The screen, as the scope's (P8h).
constexpr ImU32 kScreen = IM_COL32(8, 19, 13, 255);
constexpr ImU32 kGrid = IM_COL32(43, 66, 52, 255);
constexpr ImU32 kGap = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kPlate = IM_COL32(12, 16, 14, 255);
constexpr ImU32 kPlateEdge = IM_COL32(43, 66, 52, 255);
constexpr ImU32 kKeyLine = IM_COL32(74, 85, 80, 255);
constexpr ImU32 kKeyLit = IM_COL32(58, 74, 66, 255);
constexpr ImU32 kKeyText = IM_COL32(150, 165, 158, 255);
constexpr ImU32 kKeyTextLit = IM_COL32(232, 239, 233, 255);
constexpr ImU32 kKeyInk = IM_COL32(26, 26, 26, 255);
constexpr ImU32 kReadingText = IM_COL32(184, 196, 188, 255);
constexpr ImU32 kFaintText = IM_COL32(93, 107, 99, 255);
constexpr ImU32 kUnknown = IM_COL32(140, 140, 140, 110);
constexpr ImU32 kOverviewBar = IM_COL32(20, 26, 23, 255);
constexpr ImU32 kOverviewBox = IM_COL32(230, 230, 230, 255);
constexpr int kDimmed = 90;      // a lane's alpha while armed: the Capture shown is the last one
constexpr float kPlateKey = 28;  // a plate's keys' hit boxes, a finger's (kControl)
constexpr float kBitKey = 28, kBusKey = 44, kCloseKey = 24;
constexpr std::array<const char*, 5> kConditions{"any", "0", "1", "rise", "fall"};
constexpr std::array<TriggerPosition, 3> kPositions{TriggerPosition::Early, TriggerPosition::Middle,
                                                    TriggerPosition::Late};
constexpr std::array<const char*, 3> kPositionNames{"EARLY", "MID", "LATE"};
constexpr const char* kEmptyHint = "Clip a pin: the analyzer in hand, click a chip's pin.";

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

ImU32 WithAlpha(ImU32 colour, int alpha) {
    return (colour & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

// The sample periods shortest first, for SAMPLE's grid.
const std::array<Time, kAnalyzerPeriods.size()>& PeriodsShortestFirst() {
    static const auto periods = [] {
        std::array<Time, kAnalyzerPeriods.size()> out{};
        std::ranges::reverse_copy(kAnalyzerPeriods, out.begin());
        return out;
    }();
    return periods;
}

// A key on the screen (the corner's, a plate's): outlined, or filled when lit.
void ScreenKey(ImDrawList& list, ImVec2 min, ImVec2 max, bool lit, ImU32 fill = kKeyLit) {
    if (lit) {
        list.AddRectFilled(min, max, fill, 4);
    } else {
        list.AddRect(min, max, kKeyLine, 4);
    }
}

void CentredText(ImDrawList& list, ImVec2 min, ImVec2 max, ImU32 colour, const char* text) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    list.AddText({(min.x + max.x - size.x) / 2, (min.y + max.y - size.y) / 2}, colour, text);
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
    // A drawn case (P8i), as the scope's: no title bar, dragged by its body, resized freely.
    const Extent normal = NormalSize(BenchObject::Analyzer);
    PlaceNext(BenchObject::Analyzer, {normal.width, normal.height});
    const Extent bench = BenchArea().extent();
    ImGui::SetNextWindowSizeConstraints(
        {std::min(kAnalyzerMinWidth, bench.width), std::min(kAnalyzerMinHeight, bench.height)},
        {FLT_MAX, FLT_MAX});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kCaseFrame);
    const bool visible =
        ImGui::Begin(kAnalyzerWindowName, &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!visible) {
        ImGui::End();
        return false;
    }
    const bool clicked = InstrumentClicked(kCaseClose);
    AnalyzerSetup& setup = session.analyzer();
    AnalyzerSettings settings = setup.settings();
    bool changed = false;
    const ImVec2 at = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const AnalyzerCaseLayout layout = LayOutAnalyzerCase(windowSize.x, windowSize.y);
    const auto abs = [&](const Box& b) {
        return Box{at.x + b.x0, at.y + b.y0, at.x + b.x1, at.y + b.y1};
    };
    ImDrawList& list = *ImGui::GetWindowDrawList();

    // The case: frame, groups and their labels, badge, bezel.
    DrawCaseFrame(list, at, windowSize.x, windowSize.y);
    for (const Box& group :
         {layout.captureGroup, layout.sampleGroup, layout.triggerAtGroup, layout.viewGroup}) {
        DrawGroup(list, at, group);
    }
    const float labelY = layout.captureGroup.y0 + 2;
    DrawGroupLabel(list, at, layout.captureGroup.x0, layout.captureGroup.x1, labelY, "CAPTURE");
    DrawGroupLabel(list, at, layout.sampleGroup.x0, layout.sampleGroup.x1, labelY, "SAMPLE");
    DrawGroupLabel(list, at, layout.triggerAtGroup.x0, layout.triggerAtGroup.x1, labelY,
                   "TRIGGER AT");
    DrawGroupLabel(list, at, layout.viewLess.x0, layout.viewMore.x1, labelY, "VIEW / DIV");
    DrawBadge(list, at, layout.badge);
    DrawBezel(list, at, layout.bezel);

    // The strip's keys: items over the layout's boxes, drawn pressed while held.
    const auto key = [&](const char* id, const Box& box, const char* tip) {
        ImGui::SetCursorScreenPos({at.x + box.x0, at.y + box.y0});
        const bool hit = ImGui::InvisibleButton(id, {box.width(), box.height()});
        ImGui::SetItemTooltip("%s", tip);
        return std::pair{hit, ImGui::IsItemActive()};
    };
    // CAPTURE: ARM, and while paused the run resumed (P8i); the phase's lamps.
    const auto [arm, armHeld] = key("##arm", layout.armKey, "arm: a Capture at the next Trigger");
    if (arm) {
        setup.arm();
        if (session.powered() && session.status().state == RunState::Paused) {
            runAsked_ = true;
        }
    }
    DrawKey(list, at, layout.armKey, "ARM", armHeld);
    const AnalyzerPhase phase = setup.phase();
    const bool armed = phase == AnalyzerPhase::Armed || phase == AnalyzerPhase::Filling;
    DrawLamp(list, at, layout.lamps[0], phase == AnalyzerPhase::Armed, "ARMED");
    DrawLamp(list, at, layout.lamps[1], phase == AnalyzerPhase::Filling, "TRIG'D");
    DrawLamp(list, at, layout.lamps[2], shown_.has_value() && !armed, "DONE");
    // SAMPLE: ◀ a longer period, ▶ a shorter; the LED opens the periods by unit.
    const auto period = static_cast<std::size_t>(
        std::ranges::find(kAnalyzerPeriods, settings.period) - kAnalyzerPeriods.begin());
    const auto [slower, slowerHeld] = key("##sample-", layout.sampleLess, "sample less often");
    const auto [faster, fasterHeld] = key("##sample+", layout.sampleMore, "sample more often");
    if (slower && period > 0 && period < kAnalyzerPeriods.size()) {
        settings.period = kAnalyzerPeriods[period - 1];
        changed = true;
    }
    if (faster && period + 1 < kAnalyzerPeriods.size()) {
        settings.period = kAnalyzerPeriods[period + 1];
        changed = true;
    }
    if (key("##sample", layout.sampleValue, "sample every…").first) {
        ImGui::OpenPopup("##sample grid");
    }
    DrawArrowKey(list, at, layout.sampleLess, false, slowerHeld);
    DrawArrowKey(list, at, layout.sampleMore, true, fasterHeld);
    DrawLed(list, at, layout.sampleValue, FormatTime(settings.period, false).c_str());
    if (const auto chosen =
            TimeGrid("##sample grid", PeriodsShortestFirst(), settings.period,
                     {at.x + layout.sampleValue.x0, at.y + layout.sampleValue.y1 + 4})) {
        settings.period = *chosen;
        changed = true;
    }
    // TRIGGER AT: EARLY | MID | LATE, the time before the Trigger in each tooltip.
    std::size_t position = 0;
    for (std::size_t i = 0; i < kPositions.size(); ++i) {
        const std::string tip =
            std::format("{} of the Capture before the Trigger",
                        FormatTime(BeforeTrigger(kPositions[i], settings.period), false));
        ImGui::PushID(static_cast<int>(i));
        if (key("##at", layout.triggerAt[i], tip.c_str()).first) {
            settings.position = kPositions[i];
            changed = true;
        }
        ImGui::PopID();
        if (settings.position == kPositions[i]) {
            position = i;
        }
    }
    DrawRocker(list, at, layout.triggerAt, kPositionNames, position);
    // VIEW / DIV: ◀ see more, ▶ look closer, about the view's centre; FIT the whole Capture.
    const auto [wider, widerHeld] = key("##view-", layout.viewLess, "see more (wheel or pinch)");
    const auto [closer, closerHeld] = key("##view+", layout.viewMore, "look closer");
    key("##view", layout.viewValue, "time per division: the wheel or a pinch on the screen");
    const auto [fit, fitHeld] = key("##fit", layout.fitKey, "the whole Capture (double-click)");
    if (shown_) {
        if (wider) {
            zoom(view_.left + view_.span() / 2, -1);
        }
        if (closer) {
            zoom(view_.left + view_.span() / 2, 1);
        }
        if (fit) {
            view_ = Fit(RecordOf(*shown_));
        }
    }
    DrawArrowKey(list, at, layout.viewLess, false, widerHeld);
    DrawArrowKey(list, at, layout.viewMore, true, closerHeld);
    DrawLed(list, at, layout.viewValue,
            shown_ ? FormatTime(view_.perDivision, false).c_str() : "-");
    DrawKey(list, at, layout.fitKey, "FIT", fitHeld);
    // The size key: minimal, normal, full.
    const auto [resize, resizeHeld] =
        key("##size", layout.sizeKey,
            PoppedOut(BenchObject::Analyzer) ? "minimal / normal size"
                                             : "minimal / normal / full size");
    if (resize) {
        NextInstrumentSize(BenchObject::Analyzer);
    }
    DrawSizeKey(list, at, layout.sizeKey, resizeHeld);

    // The screen: black, its corner's keys over the plates' column.
    const Box screen = abs(layout.screen);
    const Box rowsBox = abs(layout.rows);
    const float laneX0 = at.x + layout.laneX0, laneX1 = screen.x1;
    const float width = std::max(1.0f, laneX1 - laneX0);
    list.AddRectFilled({screen.x0, screen.y0}, {screen.x1, rowsBox.y0}, kGap);
    // PIN | BUS (P8f): what a click on a pin clips.
    const Box pin = abs(layout.pinHalf), bus = abs(layout.busHalf);
    for (const bool isBus : {false, true}) {
        const Box& half = isBus ? bus : pin;
        ImGui::SetCursorScreenPos({half.x0, half.y0});
        if (ImGui::InvisibleButton(isBus ? "##bus" : "##pin", {half.width(), half.height()})) {
            bus_ = isBus;
        }
        ImGui::SetItemTooltip(isBus ? "a click on a pin clips, or unclips, its whole bus"
                                    : "a click on a pin clips, or unclips, that pin");
    }
    list.AddRect({pin.x0, pin.y0}, {bus.x1, bus.y1}, kKeyLine, 4);
    const Box& lit = bus_ ? bus : pin;
    list.AddRectFilled({lit.x0 + 2, lit.y0 + 2}, {lit.x1 - 2, lit.y1 - 2}, kKeyLit, 3);
    CentredText(list, {pin.x0, pin.y0}, {pin.x1, pin.y1}, bus_ ? kKeyText : kKeyTextLit, "PIN");
    CentredText(list, {bus.x0, bus.y0}, {bus.x1, bus.y1}, bus_ ? kKeyTextLit : kKeyText, "BUS");
    // GROUP: the ticked pins as one bus; with a group ticked, UNGROUP (P8i).
    std::erase_if(selected_, [&](std::size_t c) {
        return !setup.clips()[c].has_value() || setup.groupOf(c).has_value();
    });
    const auto groupNamed = [&](const std::string& name) -> std::optional<std::size_t> {
        for (std::size_t g = 0; g < setup.groups().size(); ++g) {
            if (setup.groups()[g].name == name) {
                return g;
            }
        }
        return std::nullopt;
    };
    if (tickedGroup_ && !groupNamed(*tickedGroup_)) {
        tickedGroup_.reset();
    }
    const bool canGroup = selected_.size() >= 2;
    const bool canUngroup = tickedGroup_.has_value();
    const Box groupKey = abs(layout.groupKey);
    ImGui::SetCursorScreenPos({groupKey.x0, groupKey.y0});
    bool groupNow = false, ungroupNow = false;
    if (ImGui::InvisibleButton("##group", {groupKey.width(), groupKey.height()})) {
        groupNow = canGroup;
        ungroupNow = canUngroup;
    }
    ImGui::SetItemTooltip(canUngroup ? "ungroup the ticked group"
                                     : "tick two pins or more, then group them as one bus");
    ScreenKey(list, {groupKey.x0, groupKey.y0}, {groupKey.x1, groupKey.y1}, canGroup || canUngroup);
    CentredText(list, {groupKey.x0, groupKey.y0}, {groupKey.x1, groupKey.y1},
                canGroup || canUngroup ? kKeyTextLit : kKeyLine, canUngroup ? "UNGROUP" : "GROUP");
    {
        const auto clipped = static_cast<std::size_t>(
            std::ranges::count_if(setup.clips(), [](const auto& c) { return c.has_value(); }));
        const std::string count = std::format("{} of {}", clipped, kChannels);
        const Box box = abs(layout.count);
        const ImVec2 size = ImGui::CalcTextSize(count.c_str());
        list.AddText({box.x1 - size.x, (box.y0 + box.y1 - size.y) / 2},
                     ImGui::GetColorU32(kHintGrey), count.c_str());
    }

    // Both read view_ as it is when called: input below changes it before anything is drawn.
    const auto x = [&](Time t) {
        return laneX0 + static_cast<float>(static_cast<double>(t - view_.left) /
                                           static_cast<double>(view_.span()) * width);
    };
    const auto timeAt = [&](float px) {
        return view_.left + static_cast<Time>(std::llround((px - laneX0) / width *
                                                           static_cast<double>(view_.span())));
    };
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    // The overview and the axis: one item, 28 px high, a press or drag there centring the view
    // on that part of the Capture, the wheel zooming.
    const Box overview = abs(layout.overview), axis = abs(layout.axis);
    ImGui::SetCursorScreenPos({laneX0, screen.y0});
    ImGui::InvisibleButton("##overview", {width, axis.y1 - screen.y0});
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    if (shown_) {
        const TimeRange record = RecordOf(*shown_);
        if (ImGui::IsItemActive()) {
            const Time centre =
                record.from +
                static_cast<Time>(std::llround((mouse.x - overview.x0) / overview.width() *
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

    // The rows, scrolled by hand (P8i: no child window, so the corner grip keeps its corner and
    // no scrollbar narrows the lanes under the axis).
    const std::vector<Row> list_ = rows(session);
    const float rowHeight = AnalyzerRowHeight(rowsBox.height(), list_.size());
    const float pitch = rowHeight + kRowGap;
    const float total = pitch * static_cast<float>(list_.size());
    const bool overflow = total > rowsBox.height();
    const auto clampScroll = [&] {
        scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, total - rowsBox.height()));
    };
    clampScroll();
    const auto rowTop = [&](std::size_t i) {
        return rowsBox.y0 + pitch * static_cast<float>(i) - scroll_;
    };
    const auto channelsOf = [&](const Row& row) {
        return row.group && !row.member ? setup.groups()[*row.group].channels
                                        : std::vector{row.channel};
    };

    // The canvas over the lanes, to the screen's bottom, first and overlappable: the plates' and
    // the corner grip's items take the hover from it.
    ImGui::SetCursorScreenPos({laneX0, rowsBox.y0});
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##canvas", {width, rowsBox.height()});
    const TimeCanvasInput input = canvas_.read(
        width, kDivisions, shown_ && marker_ ? std::optional(x(*marker_)) : std::nullopt,
        rowsBox.y0, overflow);
    if (input.scrollY != 0) {
        scroll_ += input.scrollY;
        clampScroll();
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
        // A Marker and a cursor land on an edge of the row they are on, within 12 px: a
        // finger's (P8f) and the mouse's (P8i, as the scope's since P8h).
        const auto landed = [&](float px, float py) {
            const Time t = timeAt(px);
            if (list_.empty()) {
                return t;
            }
            const auto index = std::clamp<std::ptrdiff_t>(
                static_cast<std::ptrdiff_t>(std::floor((py - rowsBox.y0 + scroll_) / pitch)), 0,
                static_cast<std::ptrdiff_t>(list_.size()) - 1);
            const std::vector<std::size_t> channels =
                channelsOf(list_[static_cast<std::size_t>(index)]);
            if (!setup.inCapture(*shown_, channels)) {
                return t;
            }
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
            marker_ = landed(*input.marker, input.pressY);
        }
        if (input.scrub) {
            placedCursor_ = landed(*input.scrub, input.pressY);
        }
        view_ = Clamp(view_, record);
        if (input.hover) {
            cursor = landed(*input.hover, mouse.y);
        } else if (placedCursor_) {
            cursor = placedCursor_;
        }
    }
    // The plates' column: the wheel there, and two fingers on the rows, scroll them.
    const bool overPlates =
        mouse.x >= screen.x0 && mouse.x < laneX0 && rowsBox.contains(mouse.x, mouse.y);
    const bool rowsHovered = rowsBox.contains(mouse.x, mouse.y) && ImGui::IsWindowHovered();
    if (overflow && ImGui::IsWindowHovered()) {
        if (overPlates && ImGui::GetIO().MouseWheel != 0) {
            scroll_ -= ImGui::GetIO().MouseWheel * pitch;
        }
        if (rowsHovered && Touch().two) {
            scroll_ -= Touch().panY;
        }
        clampScroll();
    }

    const double period_ = shown_ ? static_cast<double>(shown_->period) : 1.0;
    const double trigger = shown_ ? static_cast<double>(shown_->trigger) : 0.0;
    const auto pointOf = [&](Time t) { return trigger + static_cast<double>(t) / period_; };
    const auto pointX = [&](double point) {
        return x(static_cast<Time>(std::llround((point - trigger) * period_)));
    };
    const double fromPoint = pointOf(view_.left);
    const double toPoint = pointOf(view_.left + view_.span());
    // The plates' values: at the cursor, else the Marker, else the Trigger (P8i).
    const Time readTime = cursor ? *cursor : marker_ ? *marker_ : 0;

    // The screen under the axis, to its bottom (P8i): background, grid, the plates' column.
    list.PushClipRect({rowsBox.x0, rowsBox.y0}, {rowsBox.x1, rowsBox.y1}, true);
    list.AddRectFilled({laneX0, rowsBox.y0}, {laneX1, rowsBox.y1}, kScreen);
    list.AddRectFilled({rowsBox.x0, rowsBox.y0}, {laneX0, rowsBox.y1}, kPlate);
    list.AddLine({laneX0 - 0.5f, rowsBox.y0}, {laneX0 - 0.5f, rowsBox.y1}, kPlateEdge);
    if (shown_) {
        list.PushClipRect({laneX0, rowsBox.y0}, {laneX1, rowsBox.y1}, true);
        DrawGrid(list, LabelledGrid(view_, width), x, rowsBox.y0, rowsBox.y1, kGrid);
        list.PopClipRect();
    }

    // One lane: runs of a value, drawn as a level (one Channel) or a bus (several).
    const auto lane = [&](const CaptureRuns& runs, float top, ImU32 colour, bool isBus,
                          std::size_t bits) {
        if (runs.start.empty()) {
            return;
        }
        const float high = top + 4, low = top + rowHeight - 4, mid = top + rowHeight / 2;
        list.PushClipRect({laneX0, std::max(top, rowsBox.y0)},
                          {laneX1, std::min(top + rowHeight, rowsBox.y1)}, true);
        std::size_t r = runs.at(std::max(fromPoint, static_cast<double>(runs.start.front())));
        std::optional<std::pair<float, float>> busy;  // merged sub-pixel runs
        const auto flush = [&] {
            if (busy) {
                list.AddRectFilled({busy->first, high}, {busy->second, low}, colour);
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
            if (isBus) {
                const bool cutLeft = x0<laneX0, cutRight = x1> laneX1;
                const float slant = std::min(3.0f, (x1 - x0) / 2);
                const float left = cutLeft ? 0.0f : slant, right = cutRight ? 0.0f : slant;
                const ImVec2 shape[6] = {{x0, mid}, {x0 + left, high}, {x1 - right, high},
                                         {x1, mid}, {x1 - right, low}, {x0 + left, low}};
                if (unknown) {
                    list.AddConvexPolyFilled(shape, 6, kUnknown);
                } else {
                    list.AddPolyline(shape, 6, colour, ImDrawFlags_Closed, 1.5f);
                }
                const std::string text = BusText(runs.value[r], unknown, bits);
                const ImVec2 size = ImGui::CalcTextSize(text.c_str());
                if (const auto textX = RunTextX(x0, x1, laneX0, laneX1, size.x, slant)) {
                    list.AddText({*textX, mid - size.y / 2}, unknown ? kKeyInk : colour,
                                 text.c_str());
                }
            } else if (unknown) {
                list.AddRectFilled({x0, high}, {x1, low}, kUnknown);
                previous = 2;
            } else {
                const int level = static_cast<int>(runs.value[r]);
                const float y = level == 1 ? high : low;
                list.AddLine({x0, y}, {x1, y}, colour, 1.5f);
                if (previous == 1 - level) {
                    list.AddLine({x0, high}, {x0, low}, colour, 1.5f);
                }
                previous = level;
            }
            ++r;
        }
        flush();
        list.PopClipRect();
    };

    // Rows: each its lane, its plate's text and keys. The top rows' boxes (a group's without its
    // members) are a dragged plate's targets.
    std::optional<std::size_t> unclipChannel, unclipGroup;
    std::optional<std::pair<std::size_t, std::size_t>> dropAt;  // a dragged plate: from, to
    std::vector<Box> topBoxes;
    std::vector<std::string> topNames;
    for (std::size_t i = 0; i < list_.size(); ++i) {
        if (!list_[i].member) {
            topBoxes.push_back({rowsBox.x0, rowTop(i), laneX0, rowTop(i) + rowHeight});
            const Row& row = list_[i];
            topNames.push_back(row.group ? setup.groups()[*row.group].name
                                         : setup.label(row.channel));
        }
    }
    const std::size_t topCount = topBoxes.size();
    if (dragging_ && dragging_->place >= topCount) {
        dragging_.reset();  // its row gone
    }
    ImGui::PushClipRect({rowsBox.x0, rowsBox.y0}, {rowsBox.x1, rowsBox.y1}, true);
    for (std::size_t i = 0; i < list_.size(); ++i) {
        const Row& row = list_[i];
        const float top = rowTop(i);
        if (top + rowHeight < rowsBox.y0 || top > rowsBox.y1) {
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const ChannelGroup* group = row.group ? &setup.groups()[*row.group] : nullptr;
        const bool isGroup = group && !row.member;
        const std::vector<std::size_t> channels = channelsOf(row);
        const ImU32 colour = RowColour(row.place);
        list.AddRectFilled({rowsBox.x0, top + rowHeight}, {rowsBox.x1, top + pitch}, kGap);
        const bool inCapture = shown_ && setup.inCapture(*shown_, channels);
        // The lane: dimmed while armed (the Capture shown is the last), empty when the Capture
        // did not record this row's pins (P8i).
        if (shown_ && inCapture) {
            lane(runs(channels), top, armed ? WithAlpha(colour, kDimmed) : colour,
                 channels.size() > 1, channels.size());
        } else if (shown_) {
            const float textY = top + (rowHeight - ImGui::GetTextLineHeight()) / 2;
            list.AddText({laneX0 + 8, textY}, kFaintText, "not in this Capture");
        }
        // The plate: swatch, fold arrow, name, value, Trigger key, ×.
        const float cy = top + rowHeight / 2;
        const float textY = cy - ImGui::GetTextLineHeight() / 2;
        const float indent = row.member ? 14.0f : 0.0f;
        const float swatchX = rowsBox.x0 + 8 + indent;
        // The swatch; with two rows or more, a tick box for grouping (P8i), a member's not.
        const bool tickable = !row.member && topCount >= 2;
        const bool ticked =
            isGroup ? tickedGroup_ == group->name : !row.member && selected_.contains(row.channel);
        if (tickable) {
            ImGui::SetCursorScreenPos(
                {rowsBox.x0 + 2, top + (rowHeight - std::max(kPlateKey, rowHeight)) / 2});
            if (ImGui::InvisibleButton("##tick", {kPlateKey, std::max(kPlateKey, rowHeight)})) {
                if (isGroup) {
                    tickedGroup_ = ticked ? std::nullopt : std::optional(group->name);
                    selected_.clear();
                } else {
                    if (!selected_.erase(row.channel)) {
                        selected_.insert(row.channel);
                    }
                    tickedGroup_.reset();
                }
            }
            ImGui::SetItemTooltip(isGroup ? "tick: then UNGROUP"
                                          : "tick two pins or more: then GROUP");
            if (ticked) {
                list.AddRectFilled({swatchX - 1, cy - 7}, {swatchX + 13, cy + 7}, colour, 2);
                list.AddPolyline(std::array<ImVec2, 3>{ImVec2{swatchX + 2, cy},
                                                       ImVec2{swatchX + 5, cy + 4},
                                                       ImVec2{swatchX + 11, cy - 4}}
                                     .data(),
                                 3, kKeyInk, 0, 2.0f);
            } else {
                list.AddRect({swatchX - 1, cy - 7}, {swatchX + 13, cy + 7}, colour, 2, 0, 1.5f);
            }
        } else {
            list.AddRectFilled({swatchX, cy - 6}, {swatchX + 12, cy + 6}, colour, 2);
        }
        float nameX = swatchX + 20;
        if (isGroup) {
            const bool unfolded = expanded_.contains(group->name);
            ImGui::SetCursorScreenPos({nameX - 4, top});
            if (ImGui::InvisibleButton("##fold", {22, std::max(kPlateKey, rowHeight)})) {
                if (unfolded) {
                    expanded_.erase(group->name);
                } else {
                    expanded_.insert(group->name);
                }
            }
            ImGui::SetItemTooltip(unfolded ? "hide its pins" : "show its pins");
            if (unfolded) {
                list.AddTriangleFilled({nameX - 1, cy - 3}, {nameX + 9, cy - 3},
                                       {nameX + 4, cy + 4}, kKeyText);
            } else {
                list.AddTriangleFilled({nameX, cy - 5}, {nameX + 8, cy}, {nameX, cy + 5}, kKeyText);
            }
            nameX += 14;
        }
        const std::string name =
            isGroup ? std::format("{}[{}:0]", group->name, group->channels.size() - 1)
                    : setup.label(row.channel);
        const float closeX = laneX0 - 6 - kCloseKey;
        const float keyW = isGroup ? kBusKey : kBitKey;
        const float keyX = closeX - 4 - keyW;
        const float keyH = std::min(24.0f, rowHeight - 4);
        // The value at the read time.
        std::string value;
        if (shown_ && inCapture) {
            const CaptureRuns& r = runs(channels);
            const double point = pointOf(readTime);
            if (!r.start.empty() && point >= r.start.front() &&
                point < static_cast<double>(r.end)) {
                const std::size_t atRun = r.at(point);
                value = isGroup ? BusText(r.value[atRun], r.unknown[atRun] != 0, channels.size())
                        : r.unknown[atRun] != 0 ? "X"
                                                : std::to_string(r.value[atRun]);
            }
        }
        const float valueRight = keyX - 6;
        const float valueWidth = ImGui::CalcTextSize(value.c_str()).x;
        list.PushClipRect({nameX, top}, {valueRight - valueWidth - 6, top + rowHeight}, true);
        list.AddText({nameX, textY}, armed ? WithAlpha(colour, 160) : colour, name.c_str());
        list.PopClipRect();
        list.AddText({valueRight - valueWidth, textY}, kReadingText, value.c_str());
        // The Trigger key: dark on any, lit orange with its condition (P8i).
        const ImVec2 keyMin{keyX, cy - keyH / 2}, keyMax{keyX + keyW, cy + keyH / 2};
        if (isGroup) {
            std::string pattern = setup.groupTrigger(*row.group);
            const bool set = pattern.find_first_not_of("xX") != std::string::npos;
            bool& refused = patternRefused_[group->name];
            const FieldLook look{.frame = set ? kTriggerMark : kPlate,
                                 .frameHovered = set ? IM_COL32(255, 170, 60, 255) : kKeyLit,
                                 .frameActive = set ? IM_COL32(255, 170, 60, 255) : kKeyLit,
                                 .text = set ? kKeyInk : kKeyText,
                                 .rounding = 4,
                                 .height = keyH};
            ImGui::SetCursorScreenPos(keyMin);
            if (NumberField("##pattern", pattern, PadKind::Pattern, keyW, refused,
                            (group->channels.size() + 3) / 4, &look)) {
                refused = !setup.setGroupTrigger(*row.group, pattern);
                settings = setup.settings();
            }
            ImGui::SetItemTooltip("the bus's Trigger: hex, x = any (E0xx)");
            if (!set) {
                list.AddRect(keyMin, keyMax, kKeyLine, 4);
            }
        } else {
            const auto condition = settings.trigger[row.channel];
            const bool set = condition != TriggerCondition::Any;
            if (!row.member) {
                ImGui::SetCursorScreenPos(
                    {keyX, top + (rowHeight - std::max(kPlateKey, rowHeight)) / 2});
                if (ImGui::InvisibleButton("##trigger", {keyW, std::max(kPlateKey, rowHeight)})) {
                    ImGui::OpenPopup("##condition");
                }
                ImGui::SetItemTooltip("this pin's Trigger");
                auto index = static_cast<std::size_t>(condition);
                if (ChoicesPopup("##condition", kConditions, index, 56)) {
                    settings.trigger[row.channel] = static_cast<TriggerCondition>(index);
                    changed = true;
                }
            }
            ScreenKey(list, keyMin, keyMax, set, kTriggerMark);
            const ImU32 ink = set ? kKeyInk : kKeyText;
            const float kx = (keyMin.x + keyMax.x) / 2;
            switch (condition) {
            case TriggerCondition::Rise:
                list.AddTriangleFilled({kx - 5, cy + 4}, {kx + 5, cy + 4}, {kx, cy - 4}, ink);
                break;
            case TriggerCondition::Fall:
                list.AddTriangleFilled({kx - 5, cy - 4}, {kx + 5, cy - 4}, {kx, cy + 4}, ink);
                break;
            case TriggerCondition::Low:
                CentredText(list, keyMin, keyMax, ink, "0");
                break;
            case TriggerCondition::High:
                CentredText(list, keyMin, keyMax, ink, "1");
                break;
            case TriggerCondition::Any:
                CentredText(list, keyMin, keyMax, ink, "\xc2\xb7");
                break;
            }
        }
        // ×: unclips the row (a group: all its members).
        if (!row.member) {
            ImGui::SetCursorScreenPos(
                {closeX - 2, top + (rowHeight - std::max(kPlateKey, rowHeight)) / 2});
            if (ImGui::InvisibleButton("##unclip", {kPlateKey, std::max(kPlateKey, rowHeight)})) {
                if (isGroup) {
                    unclipGroup = *row.group;
                } else {
                    unclipChannel = row.channel;
                }
            }
            ImGui::SetItemTooltip(isGroup ? "unclip the whole group" : "unclip");
            const ImVec2 xMin{closeX, cy - kCloseKey / 2},
                xMax{closeX + kCloseKey, cy + kCloseKey / 2};
            ScreenKey(list, xMin, xMax, false);
            CentredText(list, xMin, xMax, kKeyText, "\u00d7");
            // The plate, after its keys: dragged up or down past the slop, it moves its row
            // (P8i, as the scope's bands, P8h).
            ImGui::SetCursorScreenPos({rowsBox.x0, top});
            ImGui::InvisibleButton("##plate", {laneX0 - rowsBox.x0, rowHeight});
            const std::size_t place = row.place;
            if (ImGui::IsItemActivated()) {
                dragging_ = Dragging{place, mouse.y, false};
            }
            if (dragging_ && dragging_->place == place) {
                if (ImGui::IsItemActive() &&
                    std::abs(mouse.y - dragging_->pressY) > (FingerPointer() ? kTapSlop : 3.0f)) {
                    dragging_->moving = true;
                }
                if (ImGui::IsItemDeactivated()) {
                    if (dragging_->moving) {
                        dropAt = std::pair{place, BandAt(topBoxes, mouse.y)};
                    }
                    dragging_.reset();
                }
            }
            if (ImGui::IsItemHovered() && !dragging_) {
                ImGui::SetTooltip("drag up or down: move the row");
            }
        }
        ImGui::PopID();
    }
    ImGui::PopClipRect();
    // The row dragged, at the pointer: an outline in its colour with its name.
    if (dragging_ && dragging_->moving && dragging_->place < topCount) {
        const float dy = mouse.y - dragging_->pressY;
        const Box& from = topBoxes[dragging_->place];
        const ImU32 colour = RowColour(dragging_->place);
        list.AddRectFilled({from.x0, from.y0 + dy}, {laneX0, from.y1 + dy}, kPlate);
        list.AddRect({from.x0, from.y0 + dy}, {laneX0, from.y1 + dy}, colour, 0, 0, 2.0f);
        list.AddText({from.x0 + 28, (from.y0 + from.y1 - ImGui::GetTextLineHeight()) / 2 + dy},
                     colour, topNames[dragging_->place].c_str());
    }
    if (list_.empty()) {
        const ImVec2 size = ImGui::CalcTextSize(kEmptyHint);
        list.AddText(
            {(rowsBox.x0 + rowsBox.x1 - size.x) / 2, (rowsBox.y0 + rowsBox.y1 - size.y) / 2},
            kFaintText, kEmptyHint);
    }

    // The Trigger, the Marker, the cursor and its tag, over the lanes to the screen's bottom.
    if (shown_) {
        list.PushClipRect({laneX0, rowsBox.y0}, {laneX1, rowsBox.y1}, true);
        DrawTrigger(list, std::round(x(0)), rowsBox.y0, rowsBox.y1);
        if (marker_) {
            DrawMarker(list, std::round(x(*marker_)), rowsBox.y0, rowsBox.y1);
        }
        if (cursor) {
            const float cx = x(*cursor);
            list.AddLine({cx, rowsBox.y0}, {cx, rowsBox.y1}, kCursorLine);
            DrawCursorTag(list, cx, rowsBox.y0, laneX1, CursorTagText(*cursor, marker_));
        }
        list.PopClipRect();
    }
    list.PopClipRect();

    // The overview (the whole Capture, the view as a box, T and the Marker as ticks) and the axis.
    list.AddRectFilled({overview.x0, overview.y0}, {overview.x1, overview.y1}, kOverviewBar, 2);
    if (shown_) {
        const TimeRange record = RecordOf(*shown_);
        const auto ox = [&](Time t) {
            return overview.x0 +
                   static_cast<float>(static_cast<double>(t - record.from) /
                                      static_cast<double>(record.length()) * overview.width());
        };
        list.PushClipRect({overview.x0, overview.y0 - 2}, {overview.x1, overview.y1 + 2}, true);
        const float box0 = std::max(overview.x0, ox(view_.left));
        const float box1 = std::min(overview.x1, std::max(ox(view_.left + view_.span()), box0 + 6));
        list.AddLine({ox(0), overview.y0 - 2}, {ox(0), overview.y1 + 2}, kTriggerMark, 2.0f);
        if (marker_) {
            list.AddLine({ox(*marker_), overview.y0}, {ox(*marker_), overview.y1}, kMarkerLine,
                         2.0f);
        }
        list.AddRect({box0, overview.y0 - 1}, {box1, overview.y1 + 1}, kOverviewBox, 2.0f, 0, 1.5f);
        list.PopClipRect();
        DrawAxisLabels(list, view_, LabelledGrid(view_, width), x, axis.y0, laneX0, laneX1);
    }

    if (changed) {
        setup.setSettings(settings);
    }
    // After the drawing: an unclip, a group, an ungroup or a move changes the rows.
    if (unclipChannel) {
        setup.unclip(*unclipChannel);
    } else if (unclipGroup) {
        setup.unclipGroup(*unclipGroup);
    } else if (groupNow) {
        std::vector<std::size_t> picked;  // in row order, the top row most significant
        for (const Row& row : list_) {
            if (!row.member && !row.group && selected_.contains(row.channel)) {
                picked.push_back(row.channel);
            }
        }
        setup.group(picked);
        selected_.clear();
    } else if (ungroupNow) {
        if (const auto g = groupNamed(*tickedGroup_)) {
            setup.ungroup(*g);
        }
        tickedGroup_.reset();
    } else if (dropAt) {
        setup.move(dropAt->first, dropAt->second);
    }
    if (CaseClose()) {
        open = false;
    }
    PopOnDoubleClick(BenchObject::Analyzer);
    CornerGrip();
    PinchResize(rowsHovered);  // two fingers on the rows zoom the time and scroll them
    ImGui::End();
    return clicked;
}

}  // namespace bw
