#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "machine/machine.hpp"
#include "machine/repair.hpp"
#include "parts/catalogue.hpp"
#include "parts/key_matrix.hpp"
#include "runner/analyzer_consumer.hpp"
#include "runner/led_consumer.hpp"
#include "runner/meter_consumer.hpp"
#include "runner/probe_consumer.hpp"
#include "runner/runner.hpp"
#include "runner/scope_consumer.hpp"
#include "runner/tv_consumer.hpp"

namespace bw {

struct SolderBlob {
    Layer layer;
    Point at;  // nm
};

struct ProbeTip {
    Point at;     // nm
    Layer layer;  // Active Layer when touched: which Trace a point on two Traces reads
};

enum class RepairResult : std::uint8_t { Done, Refused, Unknown };  // Refused: powered

struct PadLead {                 // a Clip or a Hook
    std::string reference, pin;  // pin by number: "6"
    std::string label;           // "U6.6 1Y"
    Point at;                    // pad, nm
};

// Analyzer Channels read as one Bus value.
struct ChannelGroup {
    std::string name;                   // "U1 A"; hand-made: "group 1"
    std::vector<std::size_t> channels;  // bit 0 first
};

struct ClipGroupResult {
    std::size_t first;                 // a Channel of the clicked pin
    std::optional<std::size_t> group;  // nullopt: the pin had no stem partner, clipped alone
};

enum class MeterMode : std::uint8_t { Volts, Continuity };

struct TestLead {
    Point at;           // nm
    Layer layer;        // as ProbeTip
    std::string label;  // "U1.14 QA", "Trace", "via"
};

struct SwitchKey {
    std::string reference;
    std::string pc;  // IsPcKey
};

// The Nets a TV hangs on: a VIDEO connector's VIDEO and SYNC_N, or a MONITOR connector's
// VIDEO_N, HDRIVE and VDRIVE, all connected; nullopt for neither. Session, bw run --tv.
std::optional<TvNets> FindTvNets(const LoadedCircuit& circuit);
// FindTvNets found some: the launcher's framing (P7b).
bool HasTv(const LoadedCircuit& circuit);

struct LedLight {
    std::string reference;  // "D1", "DS1"
    LampShape shape;        // which lamp of the Part
    float brightness;       // 0 dark .. 1 fully lit
};

// One Machine on the bench, UI thread only. Repairs edit the Machine; the next power-on rebuilds
// the Circuit, Runner, TV and probe from it. Without a Machine (netlist without a Board): no
// Repairs, no probe, the Circuit never changes.
class Session {
public:
    Session(std::optional<Machine> machine, std::unique_ptr<LoadedCircuit> circuit);
    Session(Session&&) noexcept;  // safe: the Runner only points into *live_
    ~Session();

    const Machine* machine() const { return machine_ ? &*machine_ : nullptr; }
    const Board* drawnBoard() const { return drawn_ ? &*drawn_ : nullptr; }  // never repaired
    const LoadedCircuit& circuit() const { return *live_->circuit; }
    Runner& runner() { return *live_->runner; }
    TvConsumer* tv() { return live_->tv ? &*live_->tv : nullptr; }  // replaced on rebuild

    void powerOn(std::uint64_t seed, std::optional<Time> pauseAt = std::nullopt);
    void powerOff();
    bool powered() const { return powered_; }
    // Powered with a Dead Rail: the bench supply is in current limit.
    bool currentLimited() const { return powered_ && machine_ && machine_->power().deadRail; }
    void setSpeed(Speed speed);  // kept across rebuilds

    RepairResult repair(const RepairAction& repair);  // logs and blobs every Done
    // The bench's Reset: every Repair undone, the log and blobs cleared; leads, clips and hooks
    // kept. Refused while powered; Unknown without a Machine.
    RepairResult undoRepairs();
    std::span<const std::string> log() const { return log_; }
    std::span<const SolderBlob> blobs() const { return blobs_; }

    void touch(std::optional<ProbeTip> tip);  // nullopt: lifted
    std::optional<ProbeTip> probeTip() const { return tip_; }
    std::optional<NetId> probedNet() const { return live_->probe.touched(); }
    ProbeLamps lamps() const;  // dark when not powered

    // A Machine pad by pin number or name; nullopt without a Machine or for an unknown pad.
    std::optional<PadLead> padLead(std::string_view reference, std::string_view pin) const;

    // Lowest free Channel; nullopt without a Machine, unknown pad, or all Channels used.
    std::optional<std::size_t> clip(std::string_view reference, std::string_view pin);
    void unclip(std::size_t channel);
    std::optional<std::size_t> clipAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kChannels> clips() const { return clips_; }
    std::optional<NetId> clipNet(std::size_t channel) const { return analyzer_.nets[channel]; }
    // The pin's group on its chip: the pins sharing its name stem (StemOf; not power or
    // no-connect), at least 2, each clipped in bit order to the lowest free Channel, those already
    // clipped moved into the group. A pin with no partner is clipped alone. Errors (reason text):
    // no Machine or unknown pin, not enough free Channels.
    std::expected<ClipGroupResult, std::string> clipGroup(std::string_view reference,
                                                          std::string_view pin);
    void unclipGroup(std::size_t group);  // unclips every member
    // Hand grouping of at least 2 clipped Channels, taken out of their groups; ordered by their
    // pins' StemOf index when all have one and they differ, else as given (first = most
    // significant). nullopt: fewer than 2, or a free Channel.
    std::optional<std::size_t> group(std::vector<std::size_t> channels);
    void ungroup(std::size_t group);  // members stay clipped with their conditions
    // Sets each member's condition from a bus pattern (ParseBusPattern); false: refused.
    bool setGroupTrigger(std::size_t group, std::string_view pattern);
    std::string groupTrigger(std::size_t group) const;  // FormatBusPattern of the members
    std::span<const ChannelGroup> groups() const { return groups_; }
    std::optional<std::size_t> groupOf(std::size_t channel) const;

    void setAnalyzer(const AnalyzerSettings& settings);
    const AnalyzerSettings& analyzerSettings() const { return analyzer_.settings; }
    void arm();
    void disarm();
    AnalyzerPhase analyzerPhase() const { return live_->analyzer.phase(); }
    TripleBuffer<Capture>& captures() { return live_->analyzer.captures(); }  // replaced on rebuild

    // As clip(), with the Oscilloscope's 2 Channels.
    std::optional<std::size_t> hook(std::string_view reference, std::string_view pin);
    void unhook(std::size_t channel);
    std::optional<std::size_t> hookAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kScopeChannels> hooks() const { return hooks_; }
    std::optional<double> hookRail(std::size_t channel) const;  // RailVolts of the Hook's Net
    std::optional<NetId> hookNet(std::size_t channel) const { return scope_.nets[channel]; }
    void setScope(const ScopeSettings& settings);
    const ScopeSettings& scopeSettings() const { return scope_.settings; }
    SweepKind scopeStatus() const { return live_->scope.status(); }
    TripleBuffer<Sweep>& sweeps() { return live_->scope.sweeps(); }  // replaced on rebuild

    void setMeterMode(MeterMode mode);
    MeterMode meterMode() const { return meterMode_; }
    bool placeLead(TestLead lead);  // false: no Machine, or Continuity with the power on
    void liftLeads();
    bool blackNext() const { return blackNext_; }  // Continuity: the next click places black
    const std::optional<TestLead>& red() const { return red_; }
    const std::optional<TestLead>& black() const { return black_; }
    std::optional<NetId> meterNet() const { return live_->meter.touched(); }
    std::optional<double> volts() const;     // Volts mode; nullopt: no red Lead or no reading yet
    std::optional<bool> continuity() const;  // nullopt: a Lead missing or powered

    // Every lamp of every lit Part (LED, digit segments), netlist then lamp order; 0 when not
    // powered or not watched (an unconnected lamp).
    std::vector<LedLight> leds() const;

    // Every switch (BUTTON, KEY) of the circuit, in netlist order.
    std::vector<std::string> switches() const;
    // Switches with a PC key (`pc=`), in netlist order.
    std::vector<SwitchKey> switchKeys() const;
    // Ignored while not powered. Applied at the next slice boundary; power-off releases all.
    void setPressed(std::string_view reference, bool pressed);
    bool pressed(std::string_view reference) const { return pressed_.contains(reference); }

    // Every PADDLE Part of the circuit, in netlist order.
    std::vector<std::string> knobs() const;
    // Clamped to 0..1; any time, powered or not; kept across power-off, power-on and rebuilds.
    // False (and nothing set) for a reference that is not a PADDLE.
    bool setKnob(std::string_view reference, double turn);
    double knob(std::string_view reference) const;  // 0 until set

    // Every KEYBOARD Part of the circuit, in netlist order.
    std::vector<std::string> keyboards() const;
    // Sends `code` (0x00-kKeyboardLastCode: control codes from CTRL, ESC, RETURN, BACK SPACE,
    // and the upper-case set) to KEYBOARD `reference`, which takes it at the next slice boundary.
    // False, nothing sent: not powered, not a KEYBOARD, a code it does not send, or
    // kKeyboardWaiting keys already waiting. Keys still waiting at power-off are dropped.
    bool typeCode(std::string_view reference, std::uint8_t code);
    // typeCode with the character's KeyboardCode (text, --type); on a key matrix, the press that
    // types `key` (its key map, kKey… for the named keys), queued in simulation time (held, then
    // released). False, nothing sent: not powered, no such keyboard, no key for it, queue full.
    bool type(std::string_view reference, char32_t key);
    // `text` typed: on a key matrix as a whole (the BW-80's keyboard reads keywords out of it:
    // Zx80Presses), on a KEYBOARD one character at a time. False when nothing was sent.
    bool typeText(std::string_view reference, std::u32string_view text);
    // One press on key matrix `reference` as it is (a key, SHIFT with it or not), queued in
    // simulation time as type's are. False when nothing was sent.
    bool press(std::string_view reference, MatrixPress press);

    // Every key matrix Part (BW2001KB, the BW-2001 keyboard, P5q; BW80KB, the BW-80's, P5x),
    // in netlist order.
    std::vector<std::string> matrixKeyboards() const;
    // Which keyboard `reference` is (its look and key map); nullopt for anything else.
    std::optional<KeyMatrixKind> matrixKind(std::string_view reference) const;
    // Holds or releases key `key` (index in the Part's key table) of key matrix `reference`;
    // ignored while unpowered; every held key is released at power-off, as switch presses are.
    void setMatrixKeyHeld(std::string_view reference, std::size_t key, bool held);
    bool matrixKeyHeld(std::string_view reference, std::size_t key) const;
    // Held, or pressed now by typing (Session::type, --type, paste): drawn down.
    bool matrixKeyDown(std::string_view reference, std::size_t key) const;
    // The codes sent to `reference` since power-on, the last kTypedShown.
    std::string typed(std::string_view reference) const;
    static constexpr std::size_t kTypedShown = 16;

private:
    // What one build of the Machine runs on. Members in this order: the Runner is destroyed first.
    struct Live {
        std::unique_ptr<LoadedCircuit> circuit;
        std::optional<TvConsumer> tv;
        ProbeConsumer probe;
        AnalyzerConsumer analyzer;
        MeterConsumer meter;
        ScopeConsumer scope;
        std::optional<LedConsumer> leds;
        std::unique_ptr<Runner> runner;
    };

    std::unique_ptr<Live> makeLive(std::unique_ptr<LoadedCircuit> circuit) const;
    void touchLive();
    void configureLive();
    void recomputeContinuity();
    void leaveGroup(std::size_t channel);  // a group left with one member is erased

    std::optional<Machine> machine_;
    std::optional<Board> drawn_;
    std::unique_ptr<Live> live_;
    bool dirty_ = false;  // a Repair since live_ was built
    bool powered_ = false;
    Speed speed_ = Speed::RealTime;
    std::optional<ProbeTip> tip_;
    std::vector<std::string> log_;
    std::vector<SolderBlob> blobs_;
    std::array<std::optional<PadLead>, kChannels> clips_;
    std::vector<ChannelGroup> groups_;  // disjoint; erasing one renumbers the later ones
    AnalyzerConfig analyzer_;           // nets filled per build
    std::array<std::optional<PadLead>, kScopeChannels> hooks_;
    ScopeConfig scope_;  // nets filled per build
    MeterMode meterMode_ = MeterMode::Volts;
    std::optional<TestLead> red_, black_;
    bool blackNext_ = false;
    std::optional<bool> connected_;    // red_ and black_ on one island of the current Copper
    std::optional<double> meterRail_;  // RailVolts of the meter's Net
    std::set<std::string, std::less<>> pressed_;                // Buttons held down
    std::set<std::pair<std::string, std::size_t>> matrixHeld_;  // key matrix keys held
    std::map<std::string, double, std::less<>> knobs_;          // PADDLE turns, 0..1
    std::map<std::string, std::string, std::less<>> typed_;     // codes sent, per KEYBOARD
};

}  // namespace bw
