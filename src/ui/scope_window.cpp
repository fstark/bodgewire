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
#include "view/scope_layout.hpp"
#include "view/scope_reading.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr float kEdgeReach = 12;    // px within which a cursor or Marker lands on an edge
constexpr double kFullScale = 5.0;  // V per band: 1 V a line
constexpr ImU32 kGap = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kScreen = IM_COL32(8, 19, 13, 255);
constexpr ImU32 kGrid = IM_COL32(43, 66, 52, 255);
constexpr ImU32 kGridMinor = IM_COL32(21, 37, 27, 255);
constexpr ImU32 kPlateColour = IM_COL32(12, 16, 14, 255);
constexpr ImU32 kPlateEdge = IM_COL32(43, 66, 52, 255);
constexpr ImU32 kKeyLine = IM_COL32(74, 85, 80, 255);
constexpr ImU32 kKeyText = IM_COL32(150, 165, 158, 255);
constexpr ImU32 kReadingText = IM_COL32(184, 196, 188, 255);
constexpr ImU32 kTagFill = IM_COL32(216, 224, 218, 235);
constexpr ImU32 kTagText = IM_COL32(17, 17, 17, 255);
constexpr float kKey = 28;   // a plate's T and × (kControl)
constexpr float kInset = 5;  // a plate's keys from its edges

const char* StateText(const Session& session, bool stopped) {
    if (stopped) {
        return "stopped";
    }
    if (!session.powered()) {
        return "power off";
    }
    if (session.scope().order().empty()) {
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

ImU32 WithAlpha(ImU32 colour, int alpha) {
    return (colour & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

// The T on a plate: lit orange on the Trigger's source, its edge a small triangle (the font has
// no arrows); else an outline.
void DrawTriggerKey(ImDrawList& list, ImVec2 at, bool source, ScopeEdge edge) {
    const ImVec2 max{at.x + kKey, at.y + kKey};
    const ImVec2 text = ImGui::CalcTextSize("T");
    if (!source) {
        list.AddRect(at, max, kKeyLine, 4);
        list.AddText({at.x + (kKey - text.x) / 2, at.y + (kKey - text.y) / 2}, kKeyText, "T");
        return;
    }
    list.AddRectFilled(at, max, kTriggerMark, 4);
    const ImU32 ink = IM_COL32(26, 26, 26, 255);
    const float tx = at.x + 6, mid = at.y + kKey / 2;
    if (edge == ScopeEdge::Rise) {
        list.AddTriangleFilled({tx, mid + 4}, {tx + 8, mid + 4}, {tx + 4, mid - 4}, ink);
    } else {
        list.AddTriangleFilled({tx, mid - 4}, {tx + 8, mid - 4}, {tx + 4, mid + 4}, ink);
    }
    list.AddText({at.x + 15, at.y + (kKey - text.y) / 2}, ink, "T");
}

void DrawCloseKey(ImDrawList& list, ImVec2 at) {
    const ImVec2 text = ImGui::CalcTextSize("×");
    list.AddRect(at, {at.x + kKey, at.y + kKey}, kKeyLine, 4);
    list.AddText({at.x + (kKey - text.x) / 2, at.y + (kKey - text.y) / 2}, kKeyText, "×");
}

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
    const float width = std::max(kPlate + 40, ImGui::GetContentRegionAvail().x);
    // Divisions under the label plates (P8h): the Trigger one division right of them.
    const double lead = kPlate / width * static_cast<double>(kDivisions);
    const auto zoomBy = [&](Time anchor, int notches) {
        set(WithView(settings,
                     ZoomAbout(ScopeView(settings, lead), anchor, notches, kTimebases.front(),
                               kTimebases.back()),
                     lead));
    };
    // The centre of what shows right of the plates.
    const auto centre = [&] {
        const TimeView view = ScopeView(settings, lead);
        const Time plateEnd =
            view.left +
            static_cast<Time>(std::llround(kPlate / width * static_cast<double>(view.span())));
        return (plateEnd + view.left + view.span()) / 2;
    };

    // Header line 1 (P8f: steppers and choices, no dropdowns): the timebase, longest first, so
    // + is the finer step; its - and + zoom about the visible centre, as the wheel does about
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
    ImGui::SetItemTooltip("the Trigger back at its place (double-click the screen)");
    ImGui::SameLine(0, 24);
    ImGui::TextUnformatted(StateText(session, stopped_));

    // The screen: the axis strip, then the bands, one per Hook in the scope's order (P8h).
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float height = std::max(kAxisHeight + kMinBand, ImGui::GetContentRegionAvail().y);
    const Box screen{origin.x, origin.y, origin.x + width, origin.y + height};
    const float axisY = screen.y0;
    const float top = screen.y0 + kAxisHeight;  // the bands' top
    const float plateRight = screen.x0 + kPlate;
    const std::span<const std::size_t> order = session.scope().order();
    const std::vector<Box> bands = ScopeBands(screen, kAxisHeight, order.size());
    ImDrawList& list = *ImGui::GetWindowDrawList();

    // The screen's item first, overlappable: the plates' items and the corner grip, submitted
    // after it, take the hover from it (IsItemHovered tests the rectangle only, unless the item
    // allows overlap), so a click, wheel or double-click on a plate never reaches the screen.
    ImGui::SetCursorScreenPos({screen.x0, top});
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##screen", {width, screen.y1 - top});
    std::optional<float> markerX;
    if (marker_) {
        const TimeView before = ScopeView(settings, lead);
        markerX = screen.x0 + static_cast<float>(static_cast<double>(*marker_ - before.left) /
                                                 static_cast<double>(before.span()) * width);
    }
    const TimeCanvasInput input = canvas_.read(width, kDivisions, markerX, top);
    const auto timeAt = [&](float px) {  // ps from the Trigger, at the current settings
        const TimeView view = ScopeView(settings, lead);
        return view.left + static_cast<Time>(std::llround((px - screen.x0) / width *
                                                          static_cast<double>(view.span())));
    };

    // Input, once, before drawing: the view is the settings.
    if (input.zoom != 0) {
        zoomBy(timeAt(*input.hover), input.zoom);
    }
    if (input.pan != 0) {
        panRest_ += input.pan;
        const ScopeSettings next =
            WithView(settings, Pan(ScopeView(settings, lead), panRest_, width), lead);
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
        set(WithView(*pressed_, Pan(ScopeView(*pressed_, lead), *input.dragFromPress, width),
                     lead));
    }
    // A Marker and a cursor land on an edge of the band they are on, within 12 px: a finger's
    // (P8f) and the mouse's (P8h).
    const auto landed = [&](float px, float py) {
        const Time t = timeAt(px);
        if (!shown_ || bands.empty()) {
            return t;
        }
        const auto& samples = shown_->channels[order[BandAt(bands, py)]];
        if (samples.size() < 2) {
            return t;
        }
        const Time triggerAt = TriggerTime(*shown_);
        const double reach =
            kEdgeReach / width * static_cast<double>(ScopeView(settings, lead).span());
        const auto edge = SnapToEdge(
            std::span(samples).subspan(1), static_cast<double>(t), reach,
            [&](const Sample& sample) { return sample.time - triggerAt; });  // the first: no edge
        return edge ? static_cast<Time>(std::llround(*edge)) : t;
    };
    if (input.marker) {
        marker_ = landed(*input.marker, input.pressY);
    }
    if (input.scrub) {
        placedCursor_ = landed(*input.scrub, input.pressY);
    }
    if (input.released) {
        pressed_.reset();
    }

    // The plates' items: T, ×, then the plate itself (its press is no click on the screen).
    std::optional<std::size_t> unhookChannel;
    for (std::size_t place = 0; place < bands.size(); ++place) {
        const std::size_t c = order[place];
        const Box& band = bands[place];
        ImGui::PushID(static_cast<int>(c));
        ImGui::SetCursorScreenPos({band.x0 + kInset, band.y0 + kInset});
        if (ImGui::InvisibleButton("##T", {kKey, kKey})) {
            ScopeSettings next = settings;
            if (settings.source == c) {
                next.edge = settings.edge == ScopeEdge::Rise ? ScopeEdge::Fall : ScopeEdge::Rise;
            } else {
                next.source = c;
            }
            set(next);
        }
        ImGui::SetItemTooltip(settings.source == c ? "the Trigger: a click flips its edge"
                                                   : "a click: the Trigger on this pin");
        ImGui::SetCursorScreenPos({plateRight - kInset - kKey, band.y0 + kInset});
        if (ImGui::InvisibleButton("##unhook", {kKey, kKey})) {
            unhookChannel = c;
        }
        ImGui::SetItemTooltip("unhook");
        ImGui::SetCursorScreenPos({band.x0, band.y0});
        ImGui::InvisibleButton("##plate", {kPlate, band.height()});
        ImGui::PopID();
    }

    const TimeView view = ScopeView(settings, lead);
    const double span = static_cast<double>(view.span());
    const auto x = [&](Time t) {
        return screen.x0 + static_cast<float>(static_cast<double>(t - view.left) / span * width);
    };
    std::optional<Time> cursor;
    if (input.hover) {
        cursor = landed(*input.hover, ImGui::GetIO().MousePos.y);
    } else if (placedCursor_) {
        cursor = placedCursor_;
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

    // Drawing: black, each band its background and volt lines, the time lines across them all.
    list.PushClipRect({screen.x0, top}, {screen.x1, screen.y1}, true);
    list.AddRectFilled({screen.x0, top}, {screen.x1, screen.y1}, kGap);
    const auto voltsY = [](const Box& band, double volts) {
        return band.y1 - 4 - static_cast<float>(volts / kFullScale) * (band.height() - 8);
    };
    for (const Box& band : bands) {
        list.AddRectFilled({band.x0, band.y0}, {band.x1, band.y1}, kScreen);
        for (int v = 0; v <= 5; ++v) {
            const float y = std::round(voltsY(band, v));
            list.AddLine({band.x0, y}, {band.x1, y}, v == 0 || v == 5 ? kGrid : kGridMinor);
        }
    }
    if (bands.empty()) {
        list.AddRectFilled({screen.x0, top}, {screen.x1, screen.y1}, kScreen);
    }
    const std::vector<GridLine> grid = LabelledGrid(view, width);
    DrawGrid(list, grid, x, top, screen.y1, kGrid);
    if (bands.empty()) {
        list.AddText({screen.x0 + 8, (top + screen.y1) / 2 - 8}, ImGui::GetColorU32(kHintGrey),
                     "Hook, then click a chip pin.");
    }

    // The traces, glowing: their columns grown and faint, then sharp.
    for (std::size_t place = 0; place < bands.size(); ++place) {
        const std::size_t c = order[place];
        const Box& band = bands[place];
        const ImU32 colour = kHookColours[c];
        if (!session.powered()) {
            const float y = voltsY(band, 0.0);
            list.AddLine({screen.x0, y}, {screen.x1, y}, colour, 1.5f);
            continue;
        }
        if (!shown_ || shown_->channels[c].empty()) {
            continue;
        }
        const auto& columns = columns_[c];
        for (const float grow : {2.5f, 0.75f}) {
            const ImU32 ink = grow > 1 ? WithAlpha(colour, 60) : colour;
            for (std::size_t column = 0; column < columns.size();) {
                const auto [lo, hi] = columns[column];
                std::size_t next = column + 1;
                while (next < columns.size() && columns[next] == columns[column]) {
                    ++next;  // one rect per run of equal columns
                }
                if (lo <= hi) {
                    const float left = screen.x0 + static_cast<float>(column);
                    list.AddRectFilled({left - (grow - 0.75f), voltsY(band, hi) - grow},
                                       {screen.x0 + static_cast<float>(next) + (grow - 0.75f),
                                        voltsY(band, lo) + grow},
                                       ink);
                }
                column = next;
            }
        }
    }
    if (x(0) >= screen.x0 && x(0) <= screen.x1 && !bands.empty()) {
        DrawTrigger(list, std::round(x(0)), top, screen.y1);
    }

    // The plates: T, the pin, ×; the reading under them, and the value at the cursor or Marker.
    const std::optional<Time> readTime = cursor ? cursor : marker_;
    for (std::size_t place = 0; place < bands.size(); ++place) {
        const std::size_t c = order[place];
        const Box& band = bands[place];
        list.AddRectFilled({band.x0, band.y0}, {plateRight, band.y1}, kPlateColour);
        list.AddLine({plateRight, band.y0}, {plateRight, band.y1}, kPlateEdge);
        const float y0 = voltsY(band, 0.0);
        list.AddTriangleFilled({plateRight - 9, y0 - 5}, {plateRight - 1, y0},
                               {plateRight - 9, y0 + 5}, kHookColours[c]);
        DrawTriggerKey(list, {band.x0 + kInset, band.y0 + kInset}, settings.source == c,
                       settings.edge);
        DrawCloseKey(list, {plateRight - kInset - kKey, band.y0 + kInset});
        list.PushClipRect({band.x0, band.y0}, {plateRight - kInset - kKey - 2, band.y1}, true);
        const float lineY = band.y0 + kInset + (kKey - ImGui::GetTextLineHeight()) / 2;
        list.AddText({band.x0 + kInset + kKey + 6, lineY}, kHookColours[c],
                     session.scope().hooks()[c]->label.c_str());
        list.PopClipRect();
        Reading& reading = readings_[c];
        const bool powered = session.powered();
        Time from = 0, to = 0;
        if (shown_) {
            const Time triggerAt = TriggerTime(*shown_);
            from = std::max(shown_->start, triggerAt + view.left);
            to = std::min(shown_->start + kDivisions * shown_->timebase,
                          triggerAt + view.left + view.span());
        }
        const std::uint64_t serial = shown_ ? shown_->serial : 0;
        if (reading.serial != serial || reading.from != from || reading.to != to ||
            reading.powered != powered || reading.rail != rails[c]) {
            reading = {.text = powered && shown_
                                   ? BandReading(shown_->channels[c], from, to, rails[c])
                                   : "",
                       .serial = serial,
                       .from = from,
                       .to = to,
                       .powered = powered,
                       .rail = rails[c]};
        }
        std::string text = reading.text;
        if (readTime && powered && !rails[c]) {
            // The level under the cursor or Marker, its letter only: the plate is narrow, and the
            // reading already gives a float's or a rail's volts.
            const std::string value = valueAt(c, *readTime, rails[c]);
            if (!value.empty()) {
                text += "  " + value.substr(0, 1);
            }
        }
        list.PushClipRect({band.x0, band.y0}, {plateRight - 4, band.y1}, true);
        list.AddText({band.x0 + kInset + 2, band.y0 + kInset + kKey + 2}, kReadingText,
                     text.c_str());
        list.PopClipRect();
    }

    // Right of the plates: the Marker, the cursor and its tag.
    list.PushClipRect({plateRight + 1, top}, {screen.x1, screen.y1}, true);
    if (marker_) {
        DrawMarker(list, std::round(x(*marker_)), top, screen.y1);
    }
    if (cursor) {
        const float cx = x(*cursor);
        list.AddLine({cx, top}, {cx, screen.y1}, kCursorLine);
        const std::string tag = CursorTagText(*cursor, marker_);
        const ImVec2 size = ImGui::CalcTextSize(tag.c_str());
        const float tagWidth = size.x + 12;
        const float left = cx + 4 + tagWidth <= screen.x1 ? cx + 4 : cx - 4 - tagWidth;
        const float tagTop = top + kMarkerFlagHeight + 2;
        list.AddRectFilled({left, tagTop}, {left + tagWidth, tagTop + size.y + 4}, kTagFill, 3);
        list.AddText({left + 6, tagTop + 2}, kTagText, tag.c_str());
    }
    list.PopClipRect();
    list.PopClipRect();

    // The axis strip: none over the plates, no off-screen arrow (P8h).
    DrawAxisLabels(list, view, grid, x, axisY, plateRight, screen.x1, false);

    if (changed) {
        session.scope().setSettings(settings);
    }
    // After the settings: an unhook may move the Trigger's source (P8h).
    if (unhookChannel) {
        session.scope().unhook(*unhookChannel);
    }
    PopOnDoubleClick(BenchObject::Scope);
    CornerGrip();
    PinchResize(input.hover.has_value());  // two fingers on the screen zoom the time
    ImGui::End();
    return clicked;
}

}  // namespace bw
