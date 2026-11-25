#include "ui/scope_window.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>

#include "netlist/units.hpp"

#include <imgui.h>

#include "instruments/volts.hpp"
#include "ui/bench.hpp"
#include "ui/board_view.hpp"
#include "ui/controls.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/touch.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr float kMinBand = 50.0f;
constexpr float kEdgeReach = 12;    // px within which a finger's cursor lands on an edge
constexpr double kFullScale = 5.0;  // V per band: 1 V a division
constexpr ImU32 kScreen = IM_COL32(8, 14, 10, 255);
constexpr ImU32 kGrid = IM_COL32(60, 80, 65, 255);
constexpr ImU32 kGridMinor = IM_COL32(35, 50, 40, 255);
constexpr ImU32 kScale = IM_COL32(150, 170, 150, 255);
constexpr const char* kGestures =
    "wheel or pinch: zoom   drag: position   click: marker   hold and slide: cursor   "
    "double-click: T";

const char* StateText(const Session& session, bool stopped) {
    if (stopped) {
        return "stopped";
    }
    if (!session.powered()) {
        return "power off";
    }
    if (std::ranges::none_of(session.scope().hooks(),
                             [](const auto& h) { return h.has_value(); })) {
        return "no hooks";
    }
    switch (session.scope().status()) {
    case SweepKind::None:
        return "waiting for trigger";
    case SweepKind::Triggered:
        return "triggered";
    case SweepKind::Auto:
        return "auto";
    }
    return "";
}

std::string TimebaseText(Time timebase) { return FormatTime(timebase, false) + "/div"; }

// Power-on time of the Sweep's Trigger: its left edge is delay - timebase after it.
Time TriggerTime(const Sweep& sweep) { return sweep.start - (sweep.delay - sweep.timebase); }

}  // namespace

void ScopeWindow::preset(std::optional<Time> marker, std::optional<Time> cursor) {
    marker_ = marker;
    placedCursor_ = cursor;
}

std::string ScopeWindow::valueAt(std::size_t channel, Time t,
                                 const std::optional<double>& rail) const {
    if (!shown_ || shown_->channels[channel].empty()) {
        return "";
    }
    const Time at = TriggerTime(*shown_) + t;
    if (at < shown_->start || at > shown_->start + kDivisions * shown_->timebase) {
        return "";
    }
    if (rail) {
        return std::format("{:.1f} V", *rail);
    }
    const auto& samples = shown_->channels[channel];
    const auto it = std::ranges::upper_bound(samples, at, {}, &Sample::time);
    const Sample& sample = *(it == samples.begin() ? it : it - 1);
    if (sample.floating) {
        return std::format("Z {:.1f} V", LogicVolts(sample));
    }
    switch (sample.value) {
    case LogicValue::Zero:
        return std::format("L {:.1f} V", LogicVolts(sample));
    case LogicValue::One:
        return std::format("H {:.1f} V", LogicVolts(sample));
    default:
        return "X";
    }
}

bool ScopeWindow::draw(bool& open, Session& session) {
    if (session.scope().sweeps().update() && !stopped_) {
        shown_ = session.scope().sweeps().front();
    }
    if (!open) {
        return false;
    }
    if (!BeginInstrument(kScopeWindowName, open, BenchObject::Scope)) {
        ImGui::End();
        return false;
    }
    const bool clicked = InstrumentClicked();
    ScopeSettings settings = session.scope().settings();
    bool changed = false;
    const auto set = [&](const ScopeSettings& next) {
        if (next != settings) {
            settings = next;
            changed = true;
        }
    };
    const float width = std::max(10.0f, ImGui::GetContentRegionAvail().x);
    const auto zoomBy = [&](Time anchor, int notches) {
        set(WithView(settings, ZoomAbout(ScopeView(settings), anchor, notches, kTimebases.front(),
                                         kTimebases.back())));
    };
    const auto centre = [&] { return ScopeView(settings).left + ScopeView(settings).span() / 2; };

    // Header line 1 (P8f: steppers and choices, no dropdowns): the timebase, longest first, so
    // + is the finer step; its - and + zoom about the screen's centre, as the wheel does about
    // the pointer.
    static const std::vector<std::string> kTimebaseNames = [] {
        std::vector<std::string> names;
        for (auto it = kTimebases.rbegin(); it != kTimebases.rend(); ++it) {
            names.push_back(TimebaseText(*it));
        }
        return names;
    }();
    const std::size_t longest = kTimebases.size() - 1;
    const std::size_t stepBefore = longest - TimebaseIndex(settings.timebase).value_or(longest);
    std::size_t step = stepBefore;
    if (Stepper("##timebase", kTimebaseNames, step, 104)) {
        const int notches = static_cast<int>(step) - static_cast<int>(stepBefore);
        if (notches == 1 || notches == -1) {
            zoomBy(centre(), notches);
        } else {
            ScopeSettings next = settings;
            next.timebase = kTimebases[longest - step];
            next.delay = std::min(next.delay, kMaxDelayDivisions * next.timebase);
            set(next);
        }
    }
    ImGui::SameLine(0, 12);
    BigText("T");
    ImGui::SetItemTooltip("the Trigger: its Channel and its edge");
    ImGui::SameLine(0, 6);
    constexpr std::array<const char*, kScopeChannels> kSources{"1", "2", "3", "4"};
    std::size_t source = settings.source;
    if (Choice("##source", kSources, source, kBig)) {
        ScopeSettings next = settings;
        next.source = source;
        set(next);
    }
    ImGui::SameLine(0, 6);
    constexpr std::array<const char*, 2> kEdges{"rise", "fall"};
    std::size_t edge = settings.edge == ScopeEdge::Fall ? 1 : 0;
    if (Choice("##edge", kEdges, edge, kBig)) {
        ScopeSettings next = settings;
        next.edge = edge == 1 ? ScopeEdge::Fall : ScopeEdge::Rise;
        set(next);
    }
    ImGui::SameLine(0, 12);
    constexpr std::array<const char*, 2> kModes{"auto", "normal"};
    std::size_t mode = settings.mode == ScopeMode::Normal ? 1 : 0;
    if (Choice("##mode", kModes, mode, kBig)) {
        ScopeSettings next = settings;
        next.mode = mode == 1 ? ScopeMode::Normal : ScopeMode::Auto;
        set(next);
    }
    ImGui::SameLine(0, 12);
    if (ImGui::Button(stopped_ ? "Run" : "Stop", {56, kBig})) {
        stopped_ = !stopped_;
    }
    ImGui::SetItemTooltip(stopped_ ? "show the next Sweep" : "keep this Sweep on the screen");

    // Header line 2: position (the delay), from the number pad or dragged on the screen.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("position");
    ImGui::SameLine();
    // To the tenth of a division a drag snaps to, so 98.45 µs never reads 98.4 µs.
    std::string delay = FormatTimeStep(settings.delay, settings.timebase / 10, false);
    const Time maxDelay = kMaxDelayDivisions * settings.timebase;
    if (NumberField("##delay", delay, PadKind::Duration, 104, delayRefused_)) {
        std::erase(delay, ' ');
        const auto parsed =
            delay == "0" ? std::expected<Time, std::string>(0) : ParseDuration(delay);
        delayRefused_ = !parsed || *parsed > maxDelay;
        if (!delayRefused_) {
            ScopeSettings next = settings;
            next.delay = *parsed;
            set(next);
        }
    }
    ImGui::SetItemTooltip("time from the Trigger to its mark: 12.5us, 2ms, 0 (or drag the screen)");
    ImGui::SameLine();
    if (ImGui::Button("T", {kControl, 0})) {
        ScopeSettings next = settings;
        next.delay = 0;
        set(next);
        delayRefused_ = false;
    }
    ImGui::SetItemTooltip("the Trigger back at the first division (double-click)");
    ImGui::SameLine(0, 24);
    ImGui::TextColored(kHintGrey, "1 V/div");
    ImGui::SameLine(0, 24);
    ImGui::TextUnformatted(StateText(session, stopped_));

    // Axis strip: reserved here, drawn once the input below settles the view.
    const float axisY = ImGui::GetCursorScreenPos().y;
    ImGui::Dummy({0, kAxisHeight});

    // Screen: a band per hooked Channel, sharing the height left by the rows below.
    std::vector<std::size_t> bands;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (session.scope().hooks()[c]) {
            bands.push_back(c);
        }
    }
    const float line = ImGui::GetTextLineHeightWithSpacing();
    // Under the screen: two rows of Hooks, each as high as its x button, and the status line.
    const float footer =
        2 * (kControl + ImGui::GetStyle().ItemSpacing.y) + line + ImGui::GetStyle().ItemSpacing.y;
    const float bandHeight =
        std::max(kMinBand, (ImGui::GetContentRegionAvail().y - footer) /
                               static_cast<float>(std::max<std::size_t>(1, bands.size())));
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const float height = bandHeight * static_cast<float>(std::max<std::size_t>(1, bands.size()));
    ImGui::InvisibleButton("##screen", {width, height});
    std::optional<float> markerX;
    if (marker_) {
        const TimeView before = ScopeView(settings);
        markerX = top.x + static_cast<float>(static_cast<double>(*marker_ - before.left) /
                                             static_cast<double>(before.span()) * width);
    }
    const TimeCanvasInput input = canvas_.read(width, kDivisions, markerX, top.y);
    const auto timeAt = [&](float px) {  // ps from the Trigger, at the current settings
        const TimeView view = ScopeView(settings);
        return view.left + static_cast<Time>(std::llround((px - top.x) / width *
                                                          static_cast<double>(view.span())));
    };

    // Input, once, before drawing: the view is the settings.
    if (input.zoom != 0) {
        zoomBy(timeAt(*input.hover), input.zoom);
    }
    if (input.pan != 0) {
        panRest_ += input.pan;
        const ScopeSettings next = WithView(settings, Pan(ScopeView(settings), panRest_, width));
        if (next.delay != settings.delay) {
            set(next);
            panRest_ = 0;
        }
    }
    if (input.fit) {
        ScopeSettings next = settings;
        next.delay = 0;
        set(next);
    }
    if (input.clearMarker) {
        marker_.reset();
    }
    if (input.pressed) {
        pressed_ = settings;
    }
    if (input.dragFromPress && pressed_) {
        // From the press, so slow drags are not rounded away frame by frame.
        set(WithView(*pressed_, Pan(ScopeView(*pressed_), *input.dragFromPress, width)));
    }
    // A finger's Marker and cursor land on an edge of the band it is on, within 12 px (P8f).
    const auto landed = [&](float px) {
        const Time t = timeAt(px);
        if (!FingerPointer() || !shown_ || bands.empty()) {
            return t;
        }
        const auto band = std::clamp<std::ptrdiff_t>(
            static_cast<std::ptrdiff_t>((input.pressY - top.y) / bandHeight), 0,
            static_cast<std::ptrdiff_t>(bands.size()) - 1);
        const auto& samples = shown_->channels[bands[static_cast<std::size_t>(band)]];
        if (samples.size() < 2) {
            return t;
        }
        const Time triggerAt = TriggerTime(*shown_);
        const double reach = kEdgeReach / width * static_cast<double>(ScopeView(settings).span());
        const auto edge = SnapToEdge(
            std::span(samples).subspan(1), static_cast<double>(t), reach,
            [&](const Sample& sample) { return sample.time - triggerAt; });  // the first: no edge
        return edge ? static_cast<Time>(std::llround(*edge)) : t;
    };
    if (input.marker) {
        marker_ = landed(*input.marker);
    }
    if (input.scrub) {
        placedCursor_ = landed(*input.scrub);
    }
    if (input.released) {
        pressed_.reset();
    }
    const TimeView view = ScopeView(settings);
    const double span = static_cast<double>(view.span());
    const auto x = [&](Time t) {
        return top.x + static_cast<float>(static_cast<double>(t - view.left) / span * width);
    };
    std::optional<Time> cursor;
    if (input.hover) {
        cursor = timeAt(*input.hover);
    } else if (placedCursor_) {
        cursor = placedCursor_;
    }

    // Grid.
    list.AddRectFilled(top, {top.x + width, top.y + height}, kScreen);
    for (std::size_t b = 0; b < std::max<std::size_t>(1, bands.size()); ++b) {
        const float bandTop = top.y + bandHeight * static_cast<float>(b);
        for (int v = 1; v < 5; ++v) {
            const float y = bandTop + bandHeight * static_cast<float>(v) / 5;
            list.AddLine({top.x, y}, {top.x + width, y}, kGridMinor);
        }
        list.AddLine({top.x, bandTop}, {top.x + width, bandTop}, kGrid);
    }
    const std::vector<GridLine> grid = LabelledGrid(view, width);
    DrawGrid(list, grid, x, top.y, top.y + height, kGrid);
    if (bands.empty()) {
        list.AddText({top.x + 8, top.y + bandHeight / 2 - 8}, ImGui::GetColorU32(kHintGrey),
                     "Hook, then click a chip pin.");
    }

    // The stored Sweep at the current settings: columns where its samples fall now.
    std::array<std::optional<double>, kScopeChannels> rails;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        rails[c] = session.scope().hooks()[c] ? session.scope().rail(c) : std::nullopt;
    }
    if (shown_ && (shown_->serial != columnsSerial_ || width != columnsWidth_ ||
                   rails != columnsRail_ || view != columnsView_)) {
        columnsSerial_ = shown_->serial;
        columnsWidth_ = width;
        columnsRail_ = rails;
        columnsView_ = view;
        const auto columns = static_cast<std::size_t>(width);
        const Time triggerAt = TriggerTime(*shown_);
        const Time end = shown_->start + kDivisions * shown_->timebase;
        const auto px = [&](Time at) {  // power-on time → column, unclamped
            return static_cast<double>(at - triggerAt - view.left) / span * width;
        };
        for (std::size_t c = 0; c < kScopeChannels; ++c) {
            auto& out = columns_[c];
            out.assign(columns,
                       {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()});
            const auto& samples = shown_->channels[c];
            for (std::size_t i = 0; i < samples.size(); ++i) {
                const double p0 = px(samples[i].time);
                const double p1 = px(i + 1 < samples.size() ? samples[i + 1].time : end);
                if (p1 < 0 || p0 >= static_cast<double>(columns)) {
                    continue;  // outside the screen
                }
                const auto volts =
                    static_cast<float>(rails[c] ? *rails[c] : LogicVolts(samples[i]));
                const auto first = static_cast<std::size_t>(std::max(0.0, p0));
                const auto last =
                    static_cast<std::size_t>(std::min(p1, static_cast<double>(columns - 1)));
                for (std::size_t column = first; column <= last; ++column) {
                    out[column].first = std::min(out[column].first, volts);
                    out[column].second = std::max(out[column].second, volts);
                }
            }
        }
    }

    list.PushClipRect(top, {top.x + width, top.y + height}, true);
    for (std::size_t b = 0; b < bands.size(); ++b) {
        const std::size_t c = bands[b];
        const ImU32 colour = kHookColours[c];
        const float bandTop = top.y + bandHeight * static_cast<float>(b);
        const auto bandY = [&](double volts) {
            const float bandBottom = bandTop + bandHeight - 4;
            return bandBottom - static_cast<float>(volts / kFullScale) * (bandHeight - 8);
        };
        const std::string label = std::to_string(c + 1) + " " + session.scope().hooks()[c]->label;
        list.AddText({top.x + 4, bandTop + 2}, colour, label.c_str());
        if (!session.powered()) {
            const float y = bandY(0.0);
            list.AddLine({top.x, y}, {top.x + width, y}, colour, 1.5f);
            continue;
        }
        if (!shown_ || shown_->channels[c].empty()) {
            continue;
        }
        const auto& columns = columns_[c];
        for (std::size_t column = 0; column < columns.size();) {
            const auto [lo, hi] = columns[column];
            std::size_t next = column + 1;
            while (next < columns.size() && columns[next] == columns[column]) {
                ++next;  // one rect per run of equal columns
            }
            if (lo <= hi) {
                const float left = top.x + static_cast<float>(column);
                list.AddRectFilled({left, bandY(hi) - 0.75f},
                                   {top.x + static_cast<float>(next), bandY(lo) + 0.75f}, colour);
            }
            column = next;
        }
    }
    if (!bands.empty()) {
        list.AddText({top.x + width - 30, top.y + 2}, kScale, "5 V");
    }
    if (x(0) >= top.x && x(0) <= top.x + width) {
        DrawTrigger(list, std::round(x(0)), top.y, top.y + height);
    }
    if (marker_) {
        DrawMarker(list, std::round(x(*marker_)), top.y, top.y + height);
    }
    if (cursor) {
        list.AddLine({x(*cursor), top.y}, {x(*cursor), top.y + height}, kCursorLine);
    }
    list.PopClipRect();

    // Axis strip.
    DrawAxisLabels(list, view, grid, x, axisY, top.x, top.x + width);
    DrawOffScreenTrigger(list, view, top.x, top.x + width, axisY);

    // Hook rows, two a line: value at the cursor, else at the marker.
    const std::optional<Time> readTime = cursor ? cursor : marker_;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        ImGui::PushID(static_cast<int>(c));
        if (c % 2 == 1) {
            ImGui::SameLine(width / 2 + ImGui::GetStyle().WindowPadding.x);
        }
        const auto& hook = session.scope().hooks()[c];
        // A row as high as its x button: the swatch drawn centred on it, the text aligned.
        const ImVec2 swatch = ImGui::GetCursorScreenPos();
        ImGui::Dummy({12, kControl});
        ImGui::GetWindowDrawList()->AddRectFilled({swatch.x, swatch.y + (kControl - 12) / 2},
                                                  {swatch.x + 12, swatch.y + (kControl + 12) / 2},
                                                  kHookColours[c]);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        const float rowX = ImGui::GetCursorPosX();
        if (hook) {
            ImGui::Text("%zu  %s", c + 1, hook->label.c_str());
            const std::string value =
                readTime && session.powered() ? valueAt(c, *readTime, rails[c]) : "";
            ImGui::SameLine(rowX + 170);
            ImGui::TextColored(ImColor(kHookColours[c]), "%s", value.c_str());
        } else {
            ImGui::TextColored(kHintGrey, "%zu  free", c + 1);
        }
        ImGui::SameLine(rowX + 250);
        ImGui::BeginDisabled(!hook);
        if (ImGui::Button("×", {kControl, kControl})) {
            session.scope().unhook(c);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    StatusLine(cursor, marker_, kGestures);

    if (changed) {
        session.scope().setSettings(settings);
    }
    PopOnDoubleClick(BenchObject::Scope);
    ImGui::End();
    return clicked;
}

}  // namespace bw
