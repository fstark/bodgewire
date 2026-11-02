#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "runner/consumer.hpp"
#include "runner/dispatcher.hpp"
#include "runner/spsc_ring.hpp"
#include "sim/circuit.hpp"
#include "sim/simulator.hpp"

namespace bw {

enum class Speed : std::uint8_t { RealTime, Max };
enum class RunState : std::uint8_t { Off, Running, Paused };

struct RunnerStatus {
    RunState state;
    Time simTime;
    std::uint64_t seed;
    double ringWaitShare;  // ring waits / (runUntil + ring pushes); pacing sleeps excluded
};

// Live run: a simulation thread feeding Consumers on a devices thread through one SpscRing per
// watched Net. Owns a Simulator on the Circuit; consumers and Circuit must outlive it. Starts Off.
class Runner {
public:
    static constexpr std::size_t kRingCapacity = 1 << 16;  // Samples per watched Net

    Runner(Circuit& circuit, std::vector<Consumer*> consumers,
           std::size_t ringCapacity = kRingCapacity);
    ~Runner();
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    // Commands: any thread, applied at the next slice boundary.
    void powerOn(std::uint64_t seed);
    void powerOff();
    void pause();
    void resume();
    void setSpeed(Speed speed);
    void
    pauseAt(Time time);     // one-shot: pause at the first slice end >= time (queue before powerOn)
    void watch(NetId net);  // counted: the Net's ring is written while any watch is held
    void unwatch(NetId net);  // undoes one watch(); the slot is freed when none remain
    // Simulator::wake(component, delay) at the next slice boundary, unless Off.
    void wake(ComponentId component, Time delay = 0);

    void waitIdle();  // blocks until all commands applied, not Running, devices caught up
    RunnerStatus status() const;

private:
    enum class CommandKind : std::uint8_t {
        PowerOn,
        PowerOff,
        Pause,
        Resume,
        SetSpeed,
        PauseAt,
        Watch,
        Unwatch,
        Wake
    };
    struct Command {
        CommandKind kind;
        std::int64_t value = 0;  // seed, Speed or Time
        NetId net{};
        ComponentId component{};
    };
    struct Watched {
        NetId net;
        int count;
        Time pushedUntil;  // Samples up to here already pushed
    };

    void queue(Command command);
    void simulate(std::stop_token stop);
    void apply(const Command& command, std::stop_token stop);
    void runSlice(std::stop_token stop);
    template <class T>
    bool push(SpscRing<T>& ring, const T& value, std::stop_token stop);  // false if stopping
    void pushMarker(const Marker& marker, std::stop_token stop);
    void kickDevices();
    void rebase();
    void serveDevices(std::stop_token stop);
    void drainRings();

    Simulator sim_;
    std::vector<std::unique_ptr<SpscRing<Sample>>> rings_;  // kMaxWatched slots
    SpscRing<Marker> markers_{1024};
    Dispatcher dispatcher_;

    // simulation thread
    std::array<std::optional<Watched>, kMaxWatched> watched_;
    std::optional<Time> pauseAt_;
    Speed speed_ = Speed::RealTime;
    std::chrono::steady_clock::time_point wallBase_;
    Time simBase_ = 0;

    std::array<std::atomic<bool>, kMaxWatched> slotFree_;  // false on Watch, true after Unwatch
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Command> commands_;     // guarded by mutex_
    std::uint64_t commandsQueued_ = 0;  // guarded by mutex_
    std::atomic<std::uint64_t> commandsApplied_{0};
    std::atomic<std::uint64_t> markersPushed_{0}, markersDone_{0};
    std::atomic<std::uint64_t> devicesWake_{0};  // bumped on each marker and full ring
    std::atomic<RunState> state_{RunState::Off};
    std::atomic<Time> simTime_{0};
    std::atomic<std::uint64_t> seed_{0};
    std::atomic<std::int64_t> waitedNs_{0}, runNs_{0};
    std::jthread devices_, simulation_;
};

}  // namespace bw
