#include "ui/scope_window.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>

#include "netlist/units.hpp"

#include <imgui.h>

#include "instruments/volts.hpp"
#include "ui/board_view.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/toolbar.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr float kMinBand = 50.0f;
constexpr float kLabelGap = 60.0f;  // px between time labels
constexpr double kFullScale = 5.0;  // V per band: 1 V a division
constexpr ImU32 kScreen = IM_COL32(8, 14, 10, 255);
constexpr ImU32 kGrid = IM_COL32(60, 80, 65, 255);
constexpr ImU32 kGridMinor = IM_COL32(35, 50, 40, 255);
constexpr ImU32 kScale = IM_COL32(150, 170, 150, 255);
constexpr const char* kGestures =
    "wheel: zoom   drag: position   click: marker   right-click: clear   double-click: T";

const char* StateText(const Session& session, bool stopped) {
    if (stopped) {
        return "stopped";
    }
    if (!session.powered()) {
        return "power off";
    }
    if (std::ranges::none_of(session.hooks(), [](const auto& h) { return h.has_value(); })) {
        return "no hooks";
    }
    switch (session.scopeStatus()) {
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
    presetCursor_ = cursor;
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

void ScopeWindow::draw(bool& open, Session& session, BoardView& board) {
    if (session.sweeps().update() && !stopped_) {
        shown_ = session.sweeps().front();
    }
    focused_ = false;
    if (!open) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // Below the Multimeter's reading, as in P5e.
    ImGui::SetNextWindowSize({680, 460}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + viewport->WorkSize.x * 2 / 3 - 700, viewport->WorkPos.y + 230},
        ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Oscilloscope", &open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ScopeSettings settings = session.scopeSettings();
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

    // Header line 1.
    if (ToolButton("Hook (O)", board.tool() == Tool::Hook)) {
        board.setTool(Tool::Hook);
    }
    ImGui::SameLine(0, 16);
    if (ImGui::Button("-", {24, 0})) {
        zoomBy(centre(), -1);
    }
    ImGui::SetItemTooltip("zoom out (wheel)");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    if (ImGui::BeginCombo("##timebase", TimebaseText(settings.timebase).c_str())) {
        for (const Time timebase : kTimebases) {
            if (ImGui::Selectable(TimebaseText(timebase).c_str(), timebase == settings.timebase)) {
                ScopeSettings next = settings;
                next.timebase = timebase;
                next.delay = std::min(next.delay, kMaxDelayDivisions * timebase);
                set(next);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("+", {24, 0})) {
        zoomBy(centre(), 1);
    }
    ImGui::SetItemTooltip("zoom in (wheel)");
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(100);
    const auto triggerText = [](std::size_t t) {
        return std::format("ch{} {}", t / 2 + 1, t % 2 == 0 ? "rise" : "fall");
    };
    const std::size_t trigger = settings.source * 2 + (settings.edge == ScopeEdge::Fall ? 1 : 0);
    if (ImGui::BeginCombo("##trigger", triggerText(trigger).c_str())) {
        for (std::size_t t = 0; t < 2 * kScopeChannels; ++t) {
            if (ImGui::Selectable(triggerText(t).c_str(), t == trigger)) {
                ScopeSettings next = settings;
                next.source = t / 2;
                next.edge = t % 2 == 0 ? ScopeEdge::Rise : ScopeEdge::Fall;
                set(next);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("auto", settings.mode == ScopeMode::Auto)) {
        ScopeSettings next = settings;
        next.mode = ScopeMode::Auto;
        set(next);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("normal", settings.mode == ScopeMode::Normal)) {
        ScopeSettings next = settings;
        next.mode = ScopeMode::Normal;
        set(next);
    }
    ImGui::SameLine(0, 16);
    if (ImGui::Button(stopped_ ? "Run" : "Stop", {50, 0})) {
        stopped_ = !stopped_;
    }
    ImGui::SetItemTooltip(stopped_ ? "show the next Sweep" : "keep this Sweep on the screen");
    ImGui::SameLine(0, 12);
    ImGui::TextUnformatted(StateText(session, stopped_));

    // Header line 2: position (the delay), typed or dragged on the screen.
    ImGui::TextUnformatted("position");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (!delayEditing_) {
        const std::string text = settings.delay == 0 ? "0" : FormatTime(settings.delay, false);
        std::ranges::fill(delayText_, '\0');
        std::ranges::copy(text.substr(0, delayText_.size() - 1), delayText_.begin());
    }
    if (delayRefused_) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{0.55f, 0.1f, 0.1f, 1.0f});
    }
    ImGui::InputText("##delay", delayText_.data(), delayText_.size());
    if (delayRefused_) {
        ImGui::PopStyleColor();
    }
    ImGui::SetItemTooltip("time from the Trigger to its mark: 12.5us, 2ms, 0 (or drag the screen)");
    delayEditing_ = ImGui::IsItemActive();
    if (ImGui::IsItemEdited()) {
        delayRefused_ = false;
    }
    const Time maxDelay = kMaxDelayDivisions * settings.timebase;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::string text(delayText_.data());
        std::erase(text, ' ');
        const auto parsed = text == "0" ? std::expected<Time, std::string>(0) : ParseDuration(text);
        delayRefused_ = !parsed || *parsed > maxDelay;
        if (!delayRefused_) {
            ScopeSettings next = settings;
            next.delay = *parsed;
            set(next);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("T", {28, 0})) {
        ScopeSettings next = settings;
        next.delay = 0;
        set(next);
    }
    ImGui::SetItemTooltip("the Trigger back at the first division (double-click)");
    ImGui::SameLine(0, 24);
    ImGui::TextColored(kHintGrey, "1 V/div");

    // Axis strip: reserved here, drawn once the input below settles the view.
    const float axisY = ImGui::GetCursorScreenPos().y;
    ImGui::Dummy({0, kAxisHeight});

    // Screen: a band per hooked Channel, sharing the height left by the rows below.
    std::vector<std::size_t> bands;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (session.hooks()[c]) {
            bands.push_back(c);
        }
    }
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float footer = 3 * line + 2 * ImGui::GetStyle().ItemSpacing.y;
    const float bandHeight =
        std::max(kMinBand, (ImGui::GetContentRegionAvail().y - footer) /
                               static_cast<float>(std::max<std::size_t>(1, bands.size())));
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const float height = bandHeight * static_cast<float>(std::max<std::size_t>(1, bands.size()));
    ImGui::InvisibleButton("##screen", {width, height});
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
    const ImGuiIO& io = ImGui::GetIO();
    const float mouseX = io.MousePos.x;
    const auto timeAt = [&](float px) {  // ps from the Trigger, at the current settings
        const TimeView view = ScopeView(settings);
        return view.left + static_cast<Time>(std::llround((px - top.x) / width *
                                                          static_cast<double>(view.span())));
    };

    // Input, once, before drawing: the view is the settings.
    if (ImGui::IsItemHovered()) {
        if (const int notches = WheelNotches(io.MouseWheel, wheelRest_); notches != 0) {
            zoomBy(timeAt(mouseX), notches);
        }
        if (io.MouseWheelH != 0) {
            panRest_ += -io.MouseWheelH * width / static_cast<float>(kDivisions) / 2;
            const ScopeSettings next =
                WithView(settings, Pan(ScopeView(settings), panRest_, width));
            if (next.delay != settings.delay) {
                set(next);
                panRest_ = 0;
            }
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            ScopeSettings next = settings;
            next.delay = 0;
            set(next);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            marker_.reset();
        }
    }
    if (ImGui::IsItemActivated()) {
        pressed_ = settings;
        pressX_ = mouseX;
        moved_ = false;
    }
    if (ImGui::IsItemActive() && pressed_) {
        moved_ = moved_ || std::abs(mouseX - pressX_) > 3;
        if (moved_) {
            // From the press, so slow drags are not rounded away frame by frame.
            set(WithView(*pressed_, Pan(ScopeView(*pressed_), mouseX - pressX_, width)));
        }
    }
    if (ImGui::IsItemDeactivated()) {
        if (!moved_) {
            marker_ = timeAt(mouseX);
        }
        pressed_.reset();
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        marker_.reset();
    }
    const TimeView view = ScopeView(settings);
    const double span = static_cast<double>(view.span());
    const auto x = [&](Time t) {
        return top.x + static_cast<float>(static_cast<double>(t - view.left) / span * width);
    };
    std::optional<Time> cursor;
    if (ImGui::IsItemHovered()) {
        cursor = timeAt(mouseX);
    } else if (presetCursor_) {
        cursor = presetCursor_;
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
    const std::vector<GridLine> grid = Grid(view, width, kLabelGap);
    DrawGrid(list, grid, x, top.y, top.y + height, kGrid);
    if (bands.empty()) {
        list.AddText({top.x + 8, top.y + bandHeight / 2 - 8}, ImGui::GetColorU32(kHintGrey),
                     "Hook (O), then click a chip pin.");
    }

    // The stored Sweep at the current settings: columns where its samples fall now.
    std::array<std::optional<double>, kScopeChannels> rails;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        rails[c] = session.hooks()[c] ? session.hookRail(c) : std::nullopt;
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
        const std::string label = std::to_string(c + 1) + " " + session.hooks()[c]->label;
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
    DrawAxisLabels(list, grid, x, axisY, top.x, top.x + width, OffScreenTrigger(view));
    if (const auto off = OffScreenTrigger(view)) {
        DrawOffScreenTrigger(list, *off, top.x, top.x + width, axisY);
    }

    // Hook rows, two a line: value at the cursor, else at the marker.
    const std::optional<Time> readTime = cursor ? cursor : marker_;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        ImGui::PushID(static_cast<int>(c));
        if (c % 2 == 1) {
            ImGui::SameLine(width / 2 + ImGui::GetStyle().WindowPadding.x);
        }
        const auto& hook = session.hooks()[c];
        ImGui::ColorButton("##colour", ImColor(kHookColours[c]),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {12, 12});
        ImGui::SameLine();
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
        if (ImGui::SmallButton("×")) {
            session.unhook(c);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    StatusLine(cursor, marker_, kGestures);

    focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (changed) {
        session.setScope(settings);
    }
    ImGui::End();
}

}  // namespace bw
