#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "machine/machine.hpp"
#include "machine/repair.hpp"
#include "parts/catalogue.hpp"
#include "runner/analyzer_consumer.hpp"
#include "runner/led_consumer.hpp"
#include "runner/meter_consumer.hpp"
#include "runner/probe_consumer.hpp"
#include "runner/runner.hpp"
#include "runner/scope_consumer.hpp"
#include "runner/tv_consumer.hpp"
#include "session/analyzer_setup.hpp"
#include "session/input_devices.hpp"
#include "session/leads.hpp"
#include "session/meter_setup.hpp"
#include "session/probe_setup.hpp"
#include "session/scope_setup.hpp"

namespace bw {

struct SolderBlob {
    Layer layer;
    Point at;  // nm
};

enum class RepairResult : std::uint8_t { Done, Refused, Unknown };  // Refused: powered

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

    const Machine* machine() const { return machine_.get(); }
    const Board* drawnBoard() const { return drawn_ ? &*drawn_ : nullptr; }  // never repaired
    const LoadedCircuit& circuit() const { return *live_->circuit; }
    // The current build's Runner: tests step it in simulated time (resumeUntil, waitIdle); the
    // bench goes through powerOn, pause, resume and status.
    Runner& runner() { return *live_->runner; }
    TvConsumer* tv() { return live_->tv ? &*live_->tv : nullptr; }  // replaced on rebuild

    void powerOn(std::uint64_t seed, std::optional<Time> pauseAt = std::nullopt);
    void powerOff();
    bool powered() const { return build_->powered; }
    // Powered with a Dead Rail: the bench supply is in current limit.
    bool currentLimited() const { return powered() && machine_ && machine_->power().deadRail; }
    void setSpeed(Speed speed);                 // kept across rebuilds
    void pause() { live_->runner->pause(); }    // the run switch at `‖`, powered
    void resume() { live_->runner->resume(); }  // and back
    RunnerStatus status() const { return live_->runner->status(); }

    RepairResult repair(const RepairAction& repair);  // logs and blobs every Done
    // The bench's Reset: every Repair undone, the log and blobs cleared; leads, clips and hooks
    // kept. Refused while powered; Unknown without a Machine.
    RepairResult undoRepairs();
    std::span<const std::string> log() const { return log_; }
    std::span<const SolderBlob> blobs() const { return blobs_; }

    // A Machine pad by pin number or name; nullopt without a Machine or for an unknown pad.
    std::optional<PadLead> padLead(std::string_view reference, std::string_view pin) const;

    // The Logic Analyzer's and the Oscilloscope's leads and settings.
    AnalyzerSetup& analyzer() { return analyzer_; }
    const AnalyzerSetup& analyzer() const { return analyzer_; }
    ScopeSetup& scope() { return scope_; }
    const ScopeSetup& scope() const { return scope_; }
    // The Logic Probe's tip and the Multimeter's mode and Test Leads.
    ProbeSetup& probe() { return probe_; }
    const ProbeSetup& probe() const { return probe_; }
    MeterSetup& meter() { return meter_; }
    const MeterSetup& meter() const { return meter_; }

    // Every lamp of every lit Part (LED, digit segments), netlist then lamp order; 0 when not
    // powered or not watched (an unconnected lamp).
    std::vector<LedLight> leds() const;

    // The switches, knobs, keyboards and key matrices as the player works them.
    InputDevices& inputs() { return inputs_; }
    const InputDevices& inputs() const { return inputs_; }

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
    void bindBuild();  // build_, the inputs and the instruments onto the current build

    std::unique_ptr<Machine> machine_;  // on the heap, as build_: they outlive moves
    std::unique_ptr<SessionBuild> build_ = std::make_unique<SessionBuild>();
    std::optional<Board> drawn_;
    std::unique_ptr<Live> live_;
    bool dirty_ = false;  // a Repair since live_ was built
    Speed speed_ = Speed::RealTime;
    std::vector<std::string> log_;
    std::vector<SolderBlob> blobs_;
    AnalyzerSetup analyzer_{*build_};
    ScopeSetup scope_{*build_};
    ProbeSetup probe_{*build_};
    MeterSetup meter_{*build_};
    InputDevices inputs_{*build_};
};

}  // namespace bw
