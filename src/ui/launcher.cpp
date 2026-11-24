#include "ui/launcher.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <random>
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
#include "view/pace.hpp"
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
constexpr float kNoteGap = 20;     // between the Puzzles' post-its; above them on the wood
constexpr float kNoteLeft = 24;    // "Start the Machine"
constexpr float kRowGap = 32;      // from it to the Puzzles' row
constexpr int kNoteLines = 8;      // of brief: every post-it this tall, a longer brief scrolled
constexpr float kWoodBelow = 26;   // px under the post-its: the gold bar and a margin
constexpr float kLiftShadow = 6;   // the selected post-it's, deeper
constexpr float kChevron = 18;     // px tall
constexpr float kChevronGap = 10;  // px outside the picture
constexpr auto kSettle = std::chrono::milliseconds(150);  // the selection at rest: load it
constexpr ImU32 kName = IM_COL32(235, 235, 235, 255);
constexpr ImU32 kParagraph = IM_COL32(200, 200, 205, 255);
constexpr ImU32 kErrorText = IM_COL32(255, 90, 76, 255);
constexpr ImU32 kErrorBox = IM_COL32(40, 10, 10, 230);
constexpr ImU32 kNoBoard = IM_COL32(120, 120, 128, 255);
constexpr ImU32 kSlowBar = IM_COL32(255, 128, 24, 255);    // the Machine's bar: under 1×
constexpr ImU32 kTooSlowBar = IM_COL32(230, 45, 40, 255);  // under 0.3×
constexpr ImU32 kSpeedInk = IM_COL32(140, 140, 148, 255);  // the headroom, under the paragraph
// The headroom's sentence at its longest: its room kept under every paragraph, so the text does
// not move when the measure comes.
constexpr const char* kSpeedRoom =
    "On this computer it runs at full speed: it could go 88× as fast as the real one.";
constexpr const char* kStartTitle = "Start the Machine";
constexpr const char* kStartBrief = "The Machine as it should work: no fault.";
// "Random fault" (P8d): the Machine's last post-it, a gap apart from its Puzzles, a "?" where the
// others have their brief.
constexpr float kRandomGap = 28;  // px more than the row's own gap
constexpr const char* kRandomSign = "?";
constexpr float kSignScale = 4.5f;  // of the title's size
// A large "?" over the live picture while it is selected: for the user to judge on a screenshot
// with it on and off.
constexpr bool kPictureQuestion = true;
constexpr ImU32 kPictureQuestionInk = IM_COL32(255, 255, 255, 46);
// While its Puzzle is made: ways of breaking a computer, one a second, looping. Three dots typed
// as dots (the UI font has no ellipsis). The user's to rewrite.
constexpr auto kLineTime = std::chrono::milliseconds(1000);
constexpr std::array kMakingLines{
    "Spilling coffee on the board...",     "Dropping it down the stairs...",
    "Letting the cat sleep on it...",      "Leaving it in a damp cellar for ten years...",
    "Plugging the power in backwards...",  "Lending it to a nephew...",
    "Shaking it to hear what rattles...",  "Poking around with a screwdriver, power on...",
    "Checking that it no longer works...",
};
constexpr float kLineScale = 1.6f;
constexpr ImU32 kMakingDim = IM_COL32(0, 0, 0, 150);
constexpr ImU32 kMakingPlate = IM_COL32(18, 18, 22, 240);  // behind the line
constexpr float kMakingPad = 22;
constexpr ImU32 kMakingHint = IM_COL32(170, 170, 178, 255);

std::unique_ptr<StillData> MakeStill(const std::filesystem::path& netlist) {
    if (!HasBoard(netlist)) {
        return nullptr;
    }
    auto machine = LoadNetlistMachine(netlist);
    if (!machine) {
        PrintDiagnostics(machine.error());
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
// `sign`: drawn large in place of the brief ("Random fault"'s "?", P8d).
void DrawPostIt(ImDrawList& list, ImVec2 at, float height, const std::string& title,
                const std::string& brief, bool selected, bool card, float scroll = 0,
                float overflow = 0, const char* sign = nullptr) {
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
    if (sign != nullptr) {  // centred in what is left of the note
        const float signSize = size * kNoteTitleScale * kSignScale;
        const ImVec2 extent = font->CalcTextSizeA(signSize, FLT_MAX, 0, sign);
        list.AddText(
            font, signSize,
            {at.x + (kNoteWidth - extent.x) / 2, y + (end.y - kNotePad - y - extent.y) / 2},
            WithAlpha(ink, 0.8f), sign);
    } else {
        list.AddText(font, size, {at.x + kNotePad, y}, ink, brief.c_str(), nullptr, wrap);
    }
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

// A chevron pointing left (`dir` -1) or right (1), its tip at `tip`: left and right move the lane.
void Chevron(ImDrawList& list, ImVec2 tip, float dir) {
    const float half = kChevron / 2;
    const ImVec2 points[] = {
        {tip.x - dir * half * 0.6f, tip.y - half}, tip, {tip.x - dir * half * 0.6f, tip.y + half}};
    list.AddPolyline(points, 3, kFocusGold, ImDrawFlags_None, 3.0f);
}

// The gold bar under the selected picture or post-it while its lane has the keys: one sign of the
// selection in both lanes (2026-11-23); the top lane keeps its chevrons, the keys that move it.
// Under the Machine it tells how well it keeps up (2026-11-24): gold, orange, red.
void SelectionBar(ImDrawList& list, ImVec2 at, float width, ImU32 colour = kFocusGold) {
    list.AddRectFilled({at.x, at.y + 6}, {at.x + width, at.y + 10}, colour, 2.0f);
}

ImU32 BarColour(Playability band) {
    switch (band) {
    case Playability::Playable:
        return kFocusGold;
    case Playability::Slow:
        return kSlowBar;
    case Playability::TooSlow:
        return kTooSlowBar;
    }
    return kFocusGold;
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
    std::vector<bool> random;
    for (const MachineEntry& machine : list_.machines) {
        counts.push_back(machine.puzzles.size());
        random.push_back(machine.random.has_value());
    }
    lanes_.setPuzzles(std::move(counts), std::move(random));
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
    } else if (entry->random) {  // after the Machine's Puzzles
        lanes_.selectPostIt(list_.machines[entry->machine].puzzles.size() + 1);
    }
    jump_ = true;
}

void Launcher::setError(std::string text) { error_ = std::move(text); }

std::filesystem::path Launcher::entryFile() const {
    if (list_.machines.empty()) {
        return {};
    }
    const MachineEntry& machine = list_.machines[lanes_.machine()];
    if (lanes_.random()) {
        return *machine.random;
    }
    const auto puzzle = lanes_.puzzle();
    return puzzle ? machine.puzzles[*puzzle].file : machine.netlist;
}

std::filesystem::path Launcher::liveFile() const {
    if (list_.machines.empty()) {
        return {};
    }
    return lanes_.random() ? list_.machines[lanes_.machine()].netlist : entryFile();
}

void Launcher::make(std::optional<std::uint64_t> seed) {
    if (list_.machines.empty() || !lanes_.random()) {
        return;
    }
    std::random_device device;
    const auto fresh = [&] { return (static_cast<std::uint64_t>(device()) << 32) | device(); };
    makingFile_ = entryFile();
    maker_.start(makingFile_, seed ? *seed : fresh());
    making_ = true;
    made_.reset();
    makingSince_ = std::chrono::steady_clock::now();
    firstLine_ = device() % kMakingLines.size();
    error_.clear();
}

std::optional<LauncherChoice> Launcher::updateMaking() {
    if (!making_) {
        return std::nullopt;
    }
    if (!hold_ && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        maker_.stop();
        making_ = false;
        made_.reset();
        return std::nullopt;
    }
    if (!made_) {
        if (auto result = maker_.take()) {
            if (*result) {
                made_ = std::move(**result);
            } else {
                error_ = "No random fault: " + result->error();
                making_ = false;
            }
        } else if (!maker_.making()) {
            making_ = false;  // stopped from outside: nothing made
        }
    }
    // A line is always read: the choice a second after the start at the earliest.
    if (making_ && made_ && !hold_ &&
        std::chrono::steady_clock::now() - makingSince_ >= kLineTime) {
        LauncherChoice choice{makingFile_, std::move(made_)};
        made_.reset();
        making_ = false;
        return choice;
    }
    return std::nullopt;
}

void Launcher::stop() {
    maker_.stop();
    making_ = false;
    made_.reset();
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
        return live_.loaded->session->status();
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
    std::erase_if(abandoned_, [](const std::future<OpenedMachine>& load) {
        return load.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    const std::filesystem::path want = liveFile();
    if (want != live_.file) {
        live_.view.reset();
        live_.loaded.reset();  // its runner joined
        if (live_.loading.valid()) {
            abandoned_.push_back(std::move(live_.loading));
        }
        live_.failed = false;
        live_.file = want;
        live_.since = now;
        headroom_.reset();
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
        OpenedMachine loaded = live_.loading.get();
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
    if (const RunnerStatus status = session.status(); status.state == RunState::Running) {
        headroom_.update(status.simTime, status.runNs, now);
    }
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

std::optional<LauncherChoice> Launcher::draw(TvView& tv, ImFont* legends, ImFont* titleFont) {
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
    // A Random fault being made (P8d): Esc alone does anything; its choice once made.
    std::optional<LauncherChoice> made = updateMaking();
    if (!making_ && !made) {
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
    const bool clicksOn = !chosen && !making_ && !made;
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
        // "Random fault" selected: the working Machine, a large "?" over it.
        const bool question = kPictureQuestion && i == lanes_.machine() && lanes_.random();
        if (hasTv || failed || noBoard || question) {
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
            if (question) {
                ImFont* font = ImGui::GetFont();
                const float signSize = picture.y * 0.8f;
                const ImVec2 extent = font->CalcTextSizeA(signSize, FLT_MAX, 0, kRandomSign);
                over.AddText(font, signSize,
                             {at.x + (picture.x - extent.x) / 2, at.y + (picture.y - extent.y) / 2},
                             kPictureQuestionInk, kRandomSign);
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
        // The bar under the selected picture (its colour how well the live run keeps up),
        // chevrons beside it toward the neighbours (a click moves there), while the top lane has
        // the keys.
        if (i == lanes_.machine() && lanes_.lane() == Lane::Machines && std::abs(d) < 0.5) {
            const ImU32 bar = live && headroom_.shown() ? BarColour(headroom_.band()) : kFocusGold;
            SelectionBar(list, {at.x, at.y + picture.y}, picture.x, bar);
            const float midY = at.y + picture.y / 2;
            const auto chevron = [&](float tipX, float dir, std::size_t to) {
                const ImVec2 tip{tipX, midY};
                Chevron(list, tip, dir);
                if (clicksOn) {
                    const float centre = tip.x - dir * kChevron * 0.3f;
                    ImGui::SetCursorScreenPos({centre - kChevron * 0.8f, midY - kChevron * 0.8f});
                    ImGui::PushID(dir < 0 ? "previous" : "next");
                    if (ImGui::InvisibleButton("##chevron", {kChevron * 1.6f, kChevron * 1.6f})) {
                        clickedMachine = to;
                    }
                    ImGui::PopID();
                }
            };
            if (i > 0) {
                chevron(at.x - kChevronGap - kChevron * 0.6f, -1, i - 1);
            }
            if (i + 1 < list_.machines.size()) {
                chevron(at.x + picture.x + kChevronGap + kChevron * 0.6f, 1, i + 1);
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
            // Under it, lighter, how fast the live run goes here: the selected Machine's only.
            const float speedHeight = font->CalcTextSizeA(fontSize, FLT_MAX, wrap, kSpeedRoom).y;
            float y =
                areaTop + (area.y - nameHeight - gap - lineHeight - fontSize - speedHeight) / 2;
            list.AddText(font, fontSize * kNameScale, {left, y}, WithAlpha(kName, alpha),
                         name.c_str(), nullptr, wrap);
            y += nameHeight + gap;
            list.AddText(font, fontSize, {left, y}, WithAlpha(kParagraph, alpha), line.c_str(),
                         nullptr, wrap);
            y += lineHeight + fontSize;
            if (const auto shown = headroom_.shown();
                shown && nearest == lanes_.machine() && live_.loaded) {
                const std::string speed = HeadroomText(*shown, headroom_.band());
                list.AddText(font, fontSize, {left, y}, WithAlpha(kSpeedInk, alpha), speed.c_str(),
                             nullptr, wrap);
            }
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
    // "Random fault" (P8d): the row's last post-it, kRandomGap further than the others.
    std::optional<std::size_t> randomAt;
    if (machine.random) {
        randomAt = titles.size();
        titles.emplace_back(kRandomFaultTitle);
        briefs.emplace_back();
    }
    const float apart = randomAt ? kRandomGap : 0;
    const float noteTop = laneBottom + kNoteGap;
    const float rowLeft = origin.x + kNoteLeft + kNoteWidth + kRowGap;
    const float visible = woodEnd.x - rowLeft;
    // More post-its than fit: the gap widened so the last one in view is cut about in half at the
    // window's edge, showing that the row scrolls.
    float gap = kNoteGap;
    const std::size_t puzzles = titles.size() - 1;  // the row's post-its, "Random fault" too
    if (static_cast<float>(puzzles) * (kNoteWidth + kNoteGap) - kNoteGap + apart > visible) {
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
    // Where the row's post-it `k` (1: the first Puzzle) starts, before the scroll.
    const auto rowX = [&](std::size_t k) {
        return static_cast<float>(k - 1) * pitch + (k == randomAt ? apart : 0);
    };
    if (lanes_.postIt() > 0) {
        // In view, with half of its neighbour on that side when there is one (the row still
        // shows that it scrolls).
        const std::size_t inRow = lanes_.postIt() - 1;
        const float at = rowX(lanes_.postIt());
        const float before = inRow > 0 ? kNoteWidth / 2 + gap + (lanes_.random() ? apart : 0) : 0;
        const float after = inRow + 1 < puzzles ? gap + kNoteWidth / 2 : gap;
        double target = postIts_.target;
        if (at - before - target < 0) {
            target = at - before;
        } else if (at + kNoteWidth + after - target > visible) {
            target = at + kNoteWidth + after - visible;
        }
        const float end = std::max(0.0f, static_cast<float>(puzzles) * pitch + apart - visible);
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
        DrawPostIt(list, at, noteHeight, titles[k], briefs[k], chosenOne, k == 0, scroll, overflow,
                   k == randomAt ? kRandomSign : nullptr);
        if (chosenOne && lanes_.lane() == Lane::PostIts) {
            SelectionBar(list, {at.x, at.y + noteHeight}, kNoteWidth);
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
        const float x = rowLeft + rowX(k) - static_cast<float>(postIts_.value);
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
    // "Random fault" opened: its Puzzle is made first (the choice comes from updateMaking).
    if (chosen && lanes_.random()) {
        chosen.reset();
        make();
    }
    if (making_) {
        // The wood dimmed, a line a second over it, looping from a start of its own.
        list.AddRectFilled({origin.x, laneBottom + 2}, woodEnd, kMakingDim);
        const auto waited = std::chrono::steady_clock::now() - makingSince_;
        const std::size_t line =
            (firstLine_ + static_cast<std::size_t>(waited / kLineTime)) % kMakingLines.size();
        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        const ImVec2 extent =
            font->CalcTextSizeA(fontSize * kLineScale, FLT_MAX, 0, kMakingLines[line]);
        const char* hint = "Making a random fault. Esc stops.";
        const ImVec2 hintExtent = ImGui::CalcTextSize(hint);
        // A dark plate behind the two lines: the post-its' text is still under the dimming.
        const float middle = (laneBottom + woodEnd.y) / 2;
        const float half = std::max(extent.x, hintExtent.x) / 2 + kMakingPad;
        const float centre = origin.x + width / 2;
        list.AddRectFilled({centre - half, middle - extent.y - kMakingPad},
                           {centre + half, middle + fontSize * 0.6f + hintExtent.y + kMakingPad},
                           kMakingPlate, 8.0f);
        list.AddText(font, fontSize * kLineScale, {centre - extent.x / 2, middle - extent.y}, kName,
                     kMakingLines[line]);
        list.AddText({centre - hintExtent.x / 2, middle + fontSize * 0.6f}, kMakingHint, hint);
    }
    updateLive(tv);
    // The cursor back inside the window with an item last (SetCursorScreenPos alone past the
    // content would extend it).
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({0, 0});
    if (made) {
        return made;
    }
    if (chosen) {
        return LauncherChoice{std::move(*chosen)};
    }
    return std::nullopt;
}

}  // namespace bw
