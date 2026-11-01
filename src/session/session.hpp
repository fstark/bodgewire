#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "machine/machine.hpp"
#include "machine/repair.hpp"
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

enum class MeterMode : std::uint8_t { Volts, Continuity };

struct TestLead {
    Point at;           // nm
    Layer layer;        // as ProbeTip
    std::string label;  // "U1.14 QA", "Trace", "via"
};

struct LedLight {
    std::string reference;  // "D1"
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

    // Every LED Part of the Machine, in netlist order; 0 when not powered or not in live_->leds.
    std::vector<LedLight> leds() const;

    // Every Button Part of the circuit, in netlist order.
    std::vector<std::string> buttons() const;
    // Ignored while not powered. Applied at the next slice boundary; power-off releases all.
    void setPressed(std::string_view reference, bool pressed);
    bool pressed(std::string_view reference) const { return pressed_.contains(reference); }

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
    AnalyzerConfig analyzer_;  // nets filled per build
    std::array<std::optional<PadLead>, kScopeChannels> hooks_;
    ScopeConfig scope_;  // nets filled per build
    MeterMode meterMode_ = MeterMode::Volts;
    std::optional<TestLead> red_, black_;
    bool blackNext_ = false;
    std::optional<bool> connected_;    // red_ and black_ on one island of the current Copper
    std::optional<double> meterRail_;  // RailVolts of the meter's Net
    std::set<std::string, std::less<>> pressed_;  // Buttons held down
};

}  // namespace bw
