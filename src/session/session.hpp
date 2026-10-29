#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "machine/machine.hpp"
#include "machine/repair.hpp"
#include "runner/analyzer_consumer.hpp"
#include "runner/probe_consumer.hpp"
#include "runner/runner.hpp"
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

struct Clip {
    std::string reference, pin;  // pin by number: "6"
    std::string label;           // "U6.6 1Y"
    Point at;                    // pad, nm
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
    void setSpeed(Speed speed);  // kept across rebuilds

    RepairResult repair(const RepairAction& repair);  // logs and blobs every Done
    std::span<const std::string> log() const { return log_; }
    std::span<const SolderBlob> blobs() const { return blobs_; }

    void touch(std::optional<ProbeTip> tip);  // nullopt: lifted
    std::optional<ProbeTip> probeTip() const { return tip_; }
    std::optional<NetId> probedNet() const { return live_->probe.touched(); }
    ProbeLamps lamps() const;  // dark when not powered

    // Lowest free Channel; nullopt without a Machine, unknown pad, or all Channels used.
    std::optional<std::size_t> clip(std::string_view reference, std::string_view pin);
    void unclip(std::size_t channel);
    std::optional<std::size_t> clipAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<Clip>, kChannels> clips() const { return clips_; }
    std::optional<NetId> clipNet(std::size_t channel) const { return analyzer_.nets[channel]; }
    void setAnalyzer(const AnalyzerSettings& settings);
    const AnalyzerSettings& analyzerSettings() const { return analyzer_.settings; }
    void arm();
    void disarm();
    AnalyzerPhase analyzerPhase() const { return live_->analyzer.phase(); }
    TripleBuffer<Capture>& captures() { return live_->analyzer.captures(); }  // replaced on rebuild

private:
    // What one build of the Machine runs on. Members in this order: the Runner is destroyed first.
    struct Live {
        std::unique_ptr<LoadedCircuit> circuit;
        std::optional<TvConsumer> tv;
        ProbeConsumer probe;
        AnalyzerConsumer analyzer;
        std::unique_ptr<Runner> runner;
    };

    std::unique_ptr<Live> makeLive(std::unique_ptr<LoadedCircuit> circuit) const;
    void touchLive();
    void configureLive();

    std::optional<Machine> machine_;
    std::optional<Board> drawn_;
    std::unique_ptr<Live> live_;
    bool dirty_ = false;  // a Repair since live_ was built
    bool powered_ = false;
    Speed speed_ = Speed::RealTime;
    std::optional<ProbeTip> tip_;
    std::vector<std::string> log_;
    std::vector<SolderBlob> blobs_;
    std::array<std::optional<Clip>, kChannels> clips_;
    AnalyzerConfig analyzer_;  // nets filled per build
};

}  // namespace bw
