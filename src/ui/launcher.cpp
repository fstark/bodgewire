#include "ui/launcher.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string_view>
#include <utility>

#include "machine/machine.hpp"
#include "session/session.hpp"
#include "ui/bench.hpp"
#include "ui/board_tags.hpp"
#include "ui/brief_window.hpp"
#include "ui/manual_view.hpp"
#include "ui/shelf.hpp"
#include "ui/title.hpp"
#include "ui/tv_window.hpp"
#include "view/bench_places.hpp"
#include "view/output_frame.hpp"
#include "view/tv_controls.hpp"

namespace bw {

namespace {

constexpr float kMinTopShare = 0.5f;      // of the window's height: the Machines' lane at least
constexpr float kAreaMargin = 24;         // px below the picture area
constexpr float kTitleTop = 10;           // px: the title from the window's top
constexpr float kTitleSize = 56;          // px: BODGEWIRE's font
constexpr float kTitleBand = 76;          // px: the title's band, above the picture area
constexpr float kParagraphLeft = 0.04f;   // of the width: from the picture to the paragraph
constexpr float kParagraphRight = 0.02f;  // from the paragraph to the next Machine's hint
constexpr float kMonitorShare = 0.92f;    // of the area's height: a TV Machine's monitor
constexpr float kNameScale = 1.5f;
constexpr float kNoteWidth = 190;
constexpr float kNotePad = 12;
constexpr float kNoteTitleScale = 1.2f;
constexpr float kNoteGap = 20;    // between the Puzzles' post-its; above them on the wood
constexpr float kNoteLeft = 24;   // "Start the Machine"
constexpr float kRowGap = 32;     // from it to the Puzzles' row
constexpr int kNoteLines = 8;     // of brief: every post-it this tall, a longer brief scrolled
constexpr float kWoodBelow = 26;  // px under the post-its: the gold bar and a margin
constexpr float kLiftShadow = 6;  // the selected post-it's, deeper
constexpr ImU32 kCardPaper = IM_COL32(244, 242, 235, 255);  // "Start the Machine": an index card
constexpr ImU32 kCardRed = IM_COL32(214, 96, 96, 255);      // its rule under the title
constexpr ImU32 kCardBlue = IM_COL32(150, 180, 214, 255);   // its lines
constexpr ImU32 kCardShadow = IM_COL32(0, 0, 0, 110);
constexpr float kChevron = 18;                            // px tall
constexpr float kChevronGap = 10;                         // px outside the picture
constexpr auto kSettle = std::chrono::milliseconds(150);  // the selection at rest: load it
constexpr ImU32 kName = IM_COL32(235, 235, 235, 255);
constexpr ImU32 kParagraph = IM_COL32(200, 200, 205, 255);
constexpr ImU32 kErrorText = IM_COL32(255, 90, 76, 255);
constexpr ImU32 kErrorBox = IM_COL32(40, 10, 10, 230);
constexpr ImU32 kNoBoard = IM_COL32(120, 120, 128, 255);
constexpr const char* kStartTitle = "Start the Machine";
constexpr const char* kStartBrief = "The Machine as it should work: no fault.";

std::unique_ptr<StillData> MakeStill(const std::filesystem::path& netlist) {
    if (!HasBoard(netlist)) {
        return nullptr;
    }
    auto machine = LoadNetlistMachine(netlist);
    if (!machine) {
        Print(machine.error());
        return nullptr;
    }
    const LoadedCircuit& design = machine->designCircuit();
    BoardScene scene(machine->board(), machine->pristineBoard(), design);
    const bool tv = HasTv(design);
    const Bounds frame = OutputFrame(scene, tv);
    auto leds = StillLedTags(scene, design);
    auto switches = StillSwitchTags(scene, design);
    return std::make_unique<StillData>(
        StillData{std::move(scene), std::move(leds), std::move(switches), tv, frame});
}

// `size` fitted inside `box` keeping the aspect of `aspect` (width / height).
ImVec2 Fit(ImVec2 box, double aspect) {
    if (aspect <= 0) {
        return box;
    }
    if (box.x / box.y > aspect) {
        return {static_cast<float>(box.y * aspect), box.y};
    }
    return {box.x, static_cast<float>(box.x / aspect)};
}

double Aspect(Bounds b) {
    const auto h = static_cast<double>(b.max.y - b.min.y);
    return h <= 0 ? 0 : static_cast<double>(b.max.x - b.min.x) / h;
}

ImU32 WithAlpha(ImU32 colour, float alpha) {
    const auto a = static_cast<float>((colour >> IM_COL32_A_SHIFT) & 0xFF) * alpha;
    return (colour & ~IM_COL32_A_MASK) |
           (static_cast<ImU32>(std::clamp(a, 0.0f, 255.0f)) << IM_COL32_A_SHIFT);
}

// Every post-it's height: a one-line title and kNoteLines of brief.
float FixedNoteHeight() {
    const float size = ImGui::GetFontSize();
    return kNotePad + (kNoteStrip - 4) + size * kNoteTitleScale + ImGui::GetStyle().ItemSpacing.y +
           static_cast<float>(kNoteLines) * size + kNotePad;
}

// The height a post-it's title and brief need at the note's width.
float NoteHeight(const std::string& title, const std::string& brief) {
    const float wrap = kNoteWidth - 2 * kNotePad;
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const float titleHeight =
        font->CalcTextSizeA(size * kNoteTitleScale, FLT_MAX, wrap, title.c_str()).y;
    const float briefHeight = font->CalcTextSizeA(size, FLT_MAX, wrap, brief.c_str()).y;
    return kNotePad + (kNoteStrip - 4) + titleHeight + ImGui::GetStyle().ItemSpacing.y +
           briefHeight + kNotePad;
}

// A Puzzle's post-it, or "Start the Machine"'s white index card (`card`: a red rule under the
// title, blue lines under the text). The selected one casts a deeper shadow.
// `scroll`: px of its text scrolled up, out of `overflow` (the text past the note's height): a
// small ink arrow at the bottom while more is below, at the top once scrolled.
void DrawPostIt(ImDrawList& list, ImVec2 at, float height, const std::string& title,
                const std::string& brief, bool selected, bool card, float scroll = 0,
                float overflow = 0) {
    const ImVec2 end{at.x + kNoteWidth, at.y + height};
    if (card) {
        const float shadow = selected ? kLiftShadow : 2;
        list.AddRectFilled({at.x + shadow, at.y + shadow}, {end.x + shadow, end.y + shadow},
                           kCardShadow, 2.0f);
        list.AddRectFilled(at, end, kCardPaper, 2.0f);
    } else {
        DrawNote(list, at, end, selected ? kLiftShadow : 0);
    }
    const float wrap = kNoteWidth - 2 * kNotePad;
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const ImU32 ink = ImGui::ColorConvertFloat4ToU32(kNoteInk);
    // The text clipped above the bottom margin, and below the glue strip (and its arrow, once
    // scrolled): the arrows stand in the margins, never on the text.
    list.PushClipRect({at.x, at.y + (card ? 0 : kNoteStrip) + (scroll > 0.5f ? 8 : 0)},
                      {end.x, end.y - kNotePad}, true);
    float y = at.y + kNotePad + (kNoteStrip - 4) - scroll;
    const float titleHeight =
        font->CalcTextSizeA(size * kNoteTitleScale, FLT_MAX, wrap, title.c_str()).y;
    if (card) {
        const float rule = y + titleHeight + spacing / 2;
        list.AddLine({at.x, rule}, {end.x, rule}, kCardRed, 1.0f);
        for (float line = rule + spacing / 2 + size + 1; line < end.y - 2; line += size) {
            list.AddLine({at.x, line}, {end.x, line}, kCardBlue, 1.0f);
        }
    }
    list.AddText(font, size * kNoteTitleScale, {at.x + kNotePad, y}, ink, title.c_str(), nullptr,
                 wrap);
    y += titleHeight + spacing;
    list.AddText(font, size, {at.x + kNotePad, y}, ink, brief.c_str(), nullptr, wrap);
    list.PopClipRect();
    const float midX = at.x + kNoteWidth / 2;
    const ImU32 arrow = WithAlpha(ink, 0.55f);
    if (scroll < overflow - 0.5f) {
        const float tipY = end.y - 3;
        list.AddTriangleFilled({midX - 5, tipY - 6}, {midX + 5, tipY - 6}, {midX, tipY}, arrow);
    }
    if (scroll > 0.5f) {
        const float tipY = at.y + kNoteStrip + 1;
        list.AddTriangleFilled({midX - 5, tipY + 6}, {midX + 5, tipY + 6}, {midX, tipY}, arrow);
    }
}

// A chevron pointing left (`dir` -1) or right (1), its tip at `tip`.
void Chevron(ImDrawList& list, ImVec2 tip, float dir) {
    const float half = kChevron / 2;
    const ImVec2 points[] = {
        {tip.x - dir * half * 0.6f, tip.y - half}, tip, {tip.x - dir * half * 0.6f, tip.y + half}};
    list.AddPolyline(points, 3, kFocusGold, ImDrawFlags_None, 3.0f);
}

}  // namespace

void StillLoader::start(std::vector<std::filesystem::path> netlists) {
    if (started_) {
        return;
    }
    started_ = true;
    thread_ = std::jthread([this, netlists = std::move(netlists)](std::stop_token stop) {
        for (const std::filesystem::path& netlist : netlists) {
            if (stop.stop_requested()) {
                return;
            }
            auto still = MakeStill(netlist);
            const std::scoped_lock lock(mutex_);
            done_.emplace_back(netlist, std::move(still));
        }
    });
}

std::vector<std::pair<std::filesystem::path, std::unique_ptr<StillData>>> StillLoader::take() {
    const std::scoped_lock lock(mutex_);
    return std::exchange(done_, {});
}

Launcher::~Launcher() { stop(); }

void Launcher::setList(MachineList list) {
    list_ = std::move(list);
    std::vector<std::size_t> counts;
    for (const MachineEntry& machine : list_.machines) {
        counts.push_back(machine.puzzles.size());
    }
    lanes_.setPuzzles(std::move(counts));
    jump_ = true;
}

void Launcher::select(const std::filesystem::path& file) {
    const auto entry = EntryOf(list_, file);
    if (!entry) {
        return;
    }
    lanes_.selectMachine(entry->machine);
    if (entry->puzzle) {
        lanes_.selectPostIt(*entry->puzzle + 1);
    }
    jump_ = true;
}

void Launcher::setError(std::string text) { error_ = std::move(text); }

std::filesystem::path Launcher::entryFile() const {
    if (list_.machines.empty()) {
        return {};
    }
    const MachineEntry& machine = list_.machines[lanes_.machine()];
    const auto puzzle = lanes_.puzzle();
    return puzzle ? machine.puzzles[*puzzle].file : machine.netlist;
}

void Launcher::stop() {
    live_.view.reset();
    live_.loaded.reset();
    live_.file.clear();
    live_.failed = false;
    if (live_.loading.valid()) {
        live_.loading.wait();
        live_.loading = {};
    }
    abandoned_.clear();  // each future's destructor waits for its load
}

std::optional<RunnerStatus> Launcher::liveStatus() {
    if (live_.loaded && live_.loaded->session) {
        return live_.loaded->session->runner().status();
    }
    return std::nullopt;
}

Launcher::Still* Launcher::stillOf(std::size_t machine) {
    const auto it = stills_.find(list_.machines[machine].netlist);
    return it != stills_.end() ? &it->second : nullptr;
}

bool Launcher::liveOn(std::size_t machine) const {
    return machine == lanes_.machine() && live_.loaded.has_value();
}

void Launcher::takeStills() {
    for (auto& [netlist, data] : loader_.take()) {
        if (!data) {
            continue;
        }
        Still still{data->tv, data->frame, BoardView(std::move(data->scene))};
        still.view.setStill(data->tv ? still.view.scene()->bounds() : data->frame);
        still.view.setLeds(data->leds);
        still.view.setSwitches(data->switches);
        stills_.emplace(netlist, std::move(still));
    }
}

void Launcher::updateLive(TvView& tv) {
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(abandoned_, [](const std::future<Loaded>& load) {
        return load.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    const std::filesystem::path want = entryFile();
    if (want != live_.file) {
        live_.view.reset();
        live_.loaded.reset();  // its runner joined
        if (live_.loading.valid()) {
            abandoned_.push_back(std::move(live_.loading));
        }
        live_.failed = false;
        live_.file = want;
        live_.since = now;
        tv.clear();
    }
    if (live_.file.empty()) {
        return;
    }
    if (!live_.loaded && !live_.failed && !live_.loading.valid() && now - live_.since >= kSettle) {
        live_.loading = std::async(std::launch::async,
                                   [file = live_.file] { return Load(file, std::nullopt); });
    }
    if (live_.loading.valid() &&
        live_.loading.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Loaded loaded = live_.loading.get();
        if (!loaded.session) {
            live_.failed = true;
        } else {
            live_.loaded.emplace(std::move(loaded));
            Session& session = *live_.loaded->session;
            session.powerOn(1);
            if (const Board* board = session.drawnBoard(); board && session.machine()) {
                const Machine& machine = *session.machine();
                live_.view.emplace(
                    BoardScene(*board, machine.pristineBoard(), machine.designCircuit()));
                const Still* still = stillOf(lanes_.machine());
                const BoardScene& scene = *live_.view->scene();
                const bool hasTv = session.tv() != nullptr;
                live_.view->setStill(hasTv   ? scene.bounds()
                                     : still ? still->frame
                                             : OutputFrame(scene, false));
            }
        }
    }
    if (!live_.loaded) {
        return;
    }
    Session& session = *live_.loaded->session;
    if (TvConsumer* consumer = session.tv(); consumer && consumer->pictures().update()) {
        tv.show(consumer->pictures().front());
    }
    if (live_.view && session.machine()) {
        const BoardScene& scene = *live_.view->scene();
        live_.view->setLeds(LedTagsOf(scene, session));
        live_.view->setSwitches(SwitchTagsOf(scene, session));
        ShowRepairs(*live_.view, *session.machine());
    }
}

std::optional<std::filesystem::path> Launcher::draw(TvView& tv, ImFont* legends,
                                                    ImFont* titleFont) {
    std::optional<std::filesystem::path> chosen;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float width = size.x;
    // The wood as tall as its post-its need, the Machines' lane the rest, under the title.
    const float noteHeight = FixedNoteHeight();
    const float topHeight =
        std::floor(std::max(size.y * kMinTopShare, size.y - (kNoteGap + noteHeight + kWoodBelow)));
    const float laneBottom = origin.y + topHeight;
    list.AddRectFilled(origin, {origin.x + width, laneBottom}, kPanelBackground);
    DrawTitle(list, titleFont, {origin.x + width / 2, origin.y + kTitleTop}, kTitleSize);
    if (list_.machines.empty()) {
        list.AddText({origin.x + kNoteLeft, origin.y + kTitleBand}, kName, "No Machines.");
        return std::nullopt;
    }

    // Start the stills' loads once, nearest the selection first.
    {
        std::vector<std::filesystem::path> order;
        const auto at = static_cast<std::ptrdiff_t>(lanes_.machine());
        const auto count = static_cast<std::ptrdiff_t>(list_.machines.size());
        for (std::ptrdiff_t step = 0; step < count; ++step) {
            for (const std::ptrdiff_t i : {at + step, at - step}) {
                if (i >= 0 && i < count && (step != 0 || i == at)) {
                    order.push_back(list_.machines[static_cast<std::size_t>(i)].netlist);
                }
            }
        }
        loader_.start(std::move(order));
    }
    takeStills();

    // Keys and the selection.
    const std::size_t machineBefore = lanes_.machine();
    const std::size_t postItBefore = lanes_.postIt();
    const Lane laneBefore = lanes_.lane();
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        lanes_.left();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        lanes_.right();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        lanes_.down();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        lanes_.up();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        chosen = entryFile();
    }

    const float dt = ImGui::GetIO().DeltaTime;
    if (jump_) {
        machines_.jump(static_cast<double>(lanes_.machine()));
    }
    machines_.target = static_cast<double>(lanes_.machine());
    machines_.update(dt);

    // The top lane's pictures.
    const float areaTop = origin.y + kTitleBand;
    const ImVec2 area{(static_cast<float>(kPictureTo - kPictureFrom)) * width,
                      topHeight - kTitleBand - kAreaMargin};
    const bool clicksOn = !chosen;
    std::optional<std::size_t> clickedMachine;
    bool clickedLive = false;
    std::optional<std::size_t> doubleClicked;
    for (std::size_t i = 0; i < list_.machines.size(); ++i) {
        const double d = static_cast<double>(i) - machines_.value;
        if (std::abs(d) >= 1.6 || area.x <= 0 || area.y <= 0) {
            continue;
        }
        Still* still = stillOf(i);
        const bool live = liveOn(i);
        Session* session = live ? &*live_.loaded->session : nullptr;
        const bool hasTv = still ? still->tv : session && session->tv() != nullptr;
        const ImVec2 picture = hasTv || !still ? area : Fit(area, Aspect(still->frame));
        const double share = picture.x / width;
        const ImVec2 at{origin.x + static_cast<float>(PictureLeft(d, share)) * width,
                        areaTop + (area.y - picture.y) / 2};
        BoardView* view = live && live_.view ? &*live_.view : still ? &still->view : nullptr;
        ImGui::PushID(static_cast<int>(i));
        if (view != nullptr) {
            view->setLegendFont(legends);
            ImVec2 boardSize = picture;
            if (hasTv) {  // the Board at the area's left, as tall as it, behind the monitor
                const double aspect = Aspect(view->scene()->bounds());
                boardSize = {std::min(area.x, static_cast<float>(area.y * aspect)), area.y};
            }
            ImGui::SetCursorScreenPos(at);
            view->draw(boardSize);
        }
        // Over the Board: the monitor, the load's failure, "No Board". A child of its own: the
        // launcher window's drawing would be under the Board's child.
        const bool failed = i == lanes_.machine() && live_.failed;
        const bool noBoard = live && !live_.view;
        if (hasTv || failed || noBoard) {
            ImGui::SetCursorScreenPos(at);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::BeginChild("over", picture, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();
            ImDrawList& over = *ImGui::GetWindowDrawList();
            if (hasTv) {
                const float monitorHeight = kMonitorShare * area.y;
                const float monitorWidth = monitorHeight / kTvAspect;
                TvControls controls;
                controls.setOn(live);
                DrawMonitor(
                    over, {at.x + picture.x - monitorWidth, areaTop + (area.y - monitorHeight) / 2},
                    monitorWidth, controls, live ? &tv : nullptr);
            }
            if (noBoard && !hasTv) {
                const char* text = "No Board";
                const ImVec2 extent = ImGui::CalcTextSize(text);
                over.AddText({at.x + (picture.x - extent.x) / 2, at.y + (picture.y - extent.y) / 2},
                             kNoBoard, text);
            }
            if (failed) {
                const std::string text =
                    "Could not load " + live_.file.filename().string() + ": see the terminal";
                const ImVec2 extent = ImGui::CalcTextSize(text.c_str());
                const ImVec2 min{at.x + (picture.x - extent.x) / 2 - 10,
                                 at.y + (picture.y - extent.y) / 2 - 6};
                over.AddRectFilled(min, {min.x + extent.x + 20, min.y + extent.y + 12}, kErrorBox,
                                   6.0f);
                over.AddText({min.x + 10, min.y + 6}, kErrorText, text.c_str());
            }
            ImGui::EndChild();
        }
        // The click: a neighbour moves to it, the selected one gives the top lane the keys.
        if (clicksOn) {
            ImGui::SetCursorScreenPos(at);
            if (ImGui::InvisibleButton("##picture", picture)) {
                const bool selected = i == lanes_.machine();
                if (selected && ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Left] >= 2) {
                    doubleClicked = i;  // the live one: its Machine opened, no Puzzle
                } else if (selected) {
                    clickedLive = true;
                } else {
                    clickedMachine = i;
                }
            }
        }
        // Gold chevrons beside the selected picture while the top lane has the keys.
        if (i == lanes_.machine() && lanes_.lane() == Lane::Machines && std::abs(d) < 0.5) {
            const float midY = at.y + picture.y / 2;
            if (i > 0) {
                Chevron(list, {at.x - kChevronGap - kChevron * 0.6f, midY}, -1);
            }
            if (i + 1 < list_.machines.size()) {
                Chevron(list, {at.x + picture.x + kChevronGap + kChevron * 0.6f, midY}, 1);
            }
        }
        ImGui::PopID();
    }

    // The paragraph: the nearest Machine's, fading as the lane scrolls.
    {
        const auto nearest = static_cast<std::size_t>(std::clamp(
            std::lround(machines_.value), 0L, static_cast<long>(list_.machines.size()) - 1));
        const double d = static_cast<double>(nearest) - machines_.value;
        const float alpha = std::clamp(1.0f - 2.0f * static_cast<float>(std::abs(d)), 0.0f, 1.0f);
        const Still* still = stillOf(nearest);
        const double share =
            (still && !still->tv) ? Fit(area, Aspect(still->frame)).x / width : area.x / width;
        const float shift = static_cast<float>(PictureLeft(d, share) - PictureLeft(0, share));
        const float left =
            origin.x + (static_cast<float>(kPictureTo) + kParagraphLeft + shift) * width;
        const float wrap =
            (static_cast<float>(kHintRight - kPictureTo) - kParagraphLeft - kParagraphRight) *
            width;
        if (alpha > 0 && wrap > 0) {
            const MachineEntry& machine = list_.machines[nearest];
            const std::string name = Displayable(machine.name);
            const std::string line = Displayable(machine.line);
            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            const float nameHeight =
                font->CalcTextSizeA(fontSize * kNameScale, FLT_MAX, wrap, name.c_str()).y;
            const float lineHeight = font->CalcTextSizeA(fontSize, FLT_MAX, wrap, line.c_str()).y;
            const float gap = line.empty() ? 0 : fontSize;
            float y = areaTop + (area.y - nameHeight - gap - lineHeight) / 2;
            list.AddText(font, fontSize * kNameScale, {left, y}, WithAlpha(kName, alpha),
                         name.c_str(), nullptr, wrap);
            y += nameHeight + gap;
            list.AddText(font, fontSize, {left, y}, WithAlpha(kParagraph, alpha), line.c_str(),
                         nullptr, wrap);
        }
    }

    // The bottom lane: the wood, the post-its.
    const MachineEntry& machine = list_.machines[lanes_.machine()];
    const ImVec2 woodEnd{origin.x + width, origin.y + size.y};
    list.AddRectFilled({origin.x, laneBottom}, woodEnd, kShelfWood);
    list.AddRectFilled({origin.x, laneBottom}, {woodEnd.x, laneBottom + 2}, kShelfEdge);
    if (!error_.empty()) {
        list.AddText({origin.x + kNoteLeft, laneBottom + 3}, kErrorText,
                     Displayable(error_).c_str());
    }
    std::vector<std::string> titles{kStartTitle}, briefs{kStartBrief};
    for (const PuzzleEntry& puzzle : machine.puzzles) {
        titles.push_back(Displayable(puzzle.title));
        briefs.push_back(Displayable(puzzle.brief));
    }
    const float noteTop = laneBottom + kNoteGap;
    const float rowLeft = origin.x + kNoteLeft + kNoteWidth + kRowGap;
    const float visible = woodEnd.x - rowLeft;
    // More Puzzles than fit: the gap widened so the last one in view is cut about in half at the
    // window's edge, showing that the row scrolls.
    float gap = kNoteGap;
    const std::size_t puzzles = titles.size() - 1;
    if (static_cast<float>(puzzles) * (kNoteWidth + kNoteGap) - kNoteGap > visible) {
        const float whole =
            std::max(1.0f, std::floor((visible - kNoteWidth / 2) / (kNoteWidth + kNoteGap)));
        gap = std::clamp((visible - kNoteWidth / 2) / whole - kNoteWidth, kNoteGap, 4 * kNoteGap);
    }
    const float pitch = kNoteWidth + gap;
    if (lanes_.machine() != rowMachine_) {  // another Machine's row starts at its first Puzzle
        rowMachine_ = lanes_.machine();
        postIts_.jump(0);
        noteScroll_.clear();
    }
    if (const auto puzzle = lanes_.puzzle()) {
        // In view, with half of its neighbour on that side when there is one (the row still
        // shows that it scrolls).
        const float at = static_cast<float>(*puzzle) * pitch;
        const float before = *puzzle > 0 ? kNoteWidth / 2 + gap : 0;
        const float after = *puzzle + 1 < puzzles ? gap + kNoteWidth / 2 : gap;
        double target = postIts_.target;
        if (at - before - target < 0) {
            target = at - before;
        } else if (at + kNoteWidth + after - target > visible) {
            target = at + kNoteWidth + after - visible;
        }
        const float end = std::max(0.0f, static_cast<float>(puzzles) * pitch - visible);
        postIts_.target = std::clamp(target, 0.0, static_cast<double>(end));
    }
    if (jump_) {
        postIts_.jump(postIts_.target);
    }
    postIts_.update(dt);
    jump_ = false;

    const std::size_t selected = lanes_.postIt();
    std::optional<std::size_t> clickedPostIt;
    const auto postIt = [&](std::size_t k, float x) {
        const bool chosenOne = k == selected;
        const ImVec2 at{x, noteTop};
        // A brief longer than the note: the wheel over it scrolls its text.
        noteScroll_.resize(titles.size(), 0.0f);
        const float overflow = std::max(0.0f, NoteHeight(titles[k], briefs[k]) - noteHeight);
        float& scroll = noteScroll_[k];
        if (overflow > 0 && ImGui::IsWindowHovered() &&
            ImGui::IsMouseHoveringRect(at, {at.x + kNoteWidth, at.y + noteHeight})) {
            scroll -= ImGui::GetIO().MouseWheel * ImGui::GetFontSize() * 2;
        }
        scroll = std::clamp(scroll, 0.0f, overflow);
        DrawPostIt(list, at, noteHeight, titles[k], briefs[k], chosenOne, k == 0, scroll, overflow);
        if (chosenOne && lanes_.lane() == Lane::PostIts) {
            const float barY = at.y + noteHeight + 6;
            list.AddRectFilled({at.x, barY}, {at.x + kNoteWidth, barY + 4}, kFocusGold, 2.0f);
        }
        if (clicksOn) {
            ImGui::PushID(static_cast<int>(k));
            ImGui::SetCursorScreenPos(at);
            if (ImGui::InvisibleButton("##postit", {kNoteWidth, noteHeight})) {
                clickedPostIt = k;
            }
            ImGui::PopID();
        }
    };
    ImGui::PushID("postits");
    postIt(0, origin.x + kNoteLeft);
    const float grooveX = origin.x + kNoteLeft + kNoteWidth + kRowGap / 2;
    list.AddRectFilled({grooveX - 1, laneBottom + 12}, {grooveX + 1, woodEnd.y - 12}, kTrayShadow);
    list.PushClipRect({rowLeft - kLiftShadow, laneBottom + 2}, woodEnd, true);
    ImGui::PushClipRect({rowLeft, laneBottom + 2}, woodEnd, true);
    for (std::size_t k = 1; k < titles.size(); ++k) {
        const float x =
            rowLeft + static_cast<float>(k - 1) * pitch - static_cast<float>(postIts_.value);
        if (x + kNoteWidth + kLiftShadow < rowLeft || x > woodEnd.x) {
            continue;
        }
        postIt(k, x);
    }
    ImGui::PopClipRect();
    list.PopClipRect();
    ImGui::PopID();

    // Clicks, applied after the drawing (the frame shows what was there when clicked). A post-it
    // not selected is selected (it starts running above); the selected one opens.
    if (clickedPostIt) {
        if (*clickedPostIt == lanes_.postIt()) {
            chosen = entryFile();
        }
        lanes_.selectPostIt(*clickedPostIt);
    } else if (doubleClicked) {
        lanes_.selectMachine(*doubleClicked);
        chosen = entryFile();
    } else if (clickedMachine) {
        lanes_.selectMachine(*clickedMachine);
    } else if (clickedLive) {
        lanes_.up();
    }
    if (lanes_.machine() != machineBefore || lanes_.postIt() != postItBefore ||
        lanes_.lane() != laneBefore) {
        error_.clear();
    }
    updateLive(tv);
    // The cursor back inside the window with an item last (SetCursorScreenPos alone past the
    // content would extend it).
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({0, 0});
    return chosen;
}

}  // namespace bw
