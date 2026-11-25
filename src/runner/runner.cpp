#include "runner/runner.hpp"

#include <algorithm>
#include <utility>

#include "sim/check.hpp"

namespace bw {

namespace {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr Time kSlice = 1'000'000'000;  // ps: 1 ms
constexpr auto kFullRingWait = 50us;
constexpr auto kMaxLag = 100ms;  // further behind than this: stop trying to catch up

std::chrono::nanoseconds ToWall(Time ps) { return std::chrono::nanoseconds(ps / 1000); }

std::int64_t NsSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

}  // namespace

Runner::Runner(Circuit& circuit, std::vector<Consumer*> consumers, std::size_t ringCapacity)
    : sim_(circuit), dispatcher_(consumers) {
    for (std::size_t s = 0; s < kMaxWatched; ++s) {
        rings_.push_back(std::make_unique<SpscRing<Sample>>(ringCapacity));
        slotFree_[s].store(true);
    }
    for (const Consumer* consumer : consumers) {
        for (const NetId net : consumer->nets()) {
            watch(net);
        }
    }
    devices_ = std::jthread([this](std::stop_token stop) { serveDevices(stop); });
    simulation_ = std::jthread([this](std::stop_token stop) { simulate(stop); });
}

Runner::~Runner() {
    simulation_.request_stop();
    simulation_.join();
    devices_.request_stop();
    kickDevices();
    devices_.join();
}

void Runner::queue(Command command) {
    {
        const std::scoped_lock lock(mutex_);
        commands_.push_back(std::move(command));
        ++commandsQueued_;
    }
    wake_.notify_one();
}

void Runner::powerOn(std::uint64_t seed) {
    queue({CommandKind::PowerOn, static_cast<std::int64_t>(seed)});
}
void Runner::powerOff() { queue({CommandKind::PowerOff}); }
void Runner::pause() { queue({CommandKind::Pause}); }
void Runner::resume() { queue({CommandKind::Resume}); }
void Runner::setSpeed(Speed speed) {
    queue({CommandKind::SetSpeed, static_cast<std::int64_t>(speed)});
}
void Runner::pauseAt(Time time) { queue({CommandKind::PauseAt, time}); }
void Runner::resumeUntil(Time time) { queue({CommandKind::ResumeUntil, time}); }
void Runner::watch(NetId net) { queue({CommandKind::Watch, 0, net}); }
void Runner::unwatch(NetId net) { queue({CommandKind::Unwatch, 0, net}); }
void Runner::wake(ComponentId component, Time delay) {
    queue({CommandKind::Wake, delay, NetId{}, component});
}

void Runner::post(std::function<void()> action, std::optional<ComponentId> wake, Time delay) {
    queue({CommandKind::Post, delay, NetId{}, wake.value_or(ComponentId{}), std::move(action),
           wake.has_value()});
}

void Runner::waitIdle() {
    while (true) {
        std::uint64_t queued = 0;
        {
            const std::scoped_lock lock(mutex_);
            queued = commandsQueued_;
        }
        const std::uint64_t done = noticesDone_.load();
        if (commandsApplied_.load() == queued && state_.load() != RunState::Running &&
            done == noticesPushed_.load()) {
            return;
        }
        std::this_thread::sleep_for(100us);
    }
}

RunnerStatus Runner::status() const {
    const std::int64_t runNs = runNs_.load();
    const auto run = std::max<std::int64_t>(1, runNs);
    return {state_.load(), simTime_.load(), seed_.load(),
            static_cast<double>(waitedNs_.load()) / static_cast<double>(run), runNs};
}

// --- simulation thread ---

void Runner::simulate(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::vector<Command> commands;
        {
            std::unique_lock lock(mutex_);
            if (state_.load() != RunState::Running) {
                wake_.wait(lock, stop, [&] { return !commands_.empty(); });
            }
            commands.swap(commands_);
        }
        for (const Command& command : commands) {
            apply(command, stop);
            commandsApplied_.fetch_add(1);
        }
        if (stop.stop_requested() || !commands.empty() || state_.load() != RunState::Running) {
            continue;
        }
        if (speed_ != Speed::Max) {
            const std::int64_t stretch = speed_ == Speed::Slow ? kSlowMotion : 1;
            const auto due = wallBase_ + ToWall((sim_.now() + kSlice - simBase_) * stretch);
            const auto now = Clock::now();
            if (due < now - kMaxLag) {
                wallBase_ = now - ToWall(kSlice * stretch);
                simBase_ = sim_.now();
            } else if (due > now) {
                std::unique_lock lock(mutex_);
                if (wake_.wait_until(lock, stop, due, [&] { return !commands_.empty(); })) {
                    continue;
                }
            }
        }
        runSlice(stop);
    }
}

void Runner::rebase() {
    wallBase_ = Clock::now();
    simBase_ = sim_.now();
}

void Runner::apply(const Command& command, std::stop_token stop) {
    switch (command.kind) {
    case CommandKind::PowerOn: {
        for (std::uint64_t done = noticesDone_.load(); done != noticesPushed_.load();
             done = noticesDone_.load()) {
            if (stop.stop_requested()) {
                return;
            }
            noticesDone_.wait(done);
        }
        const auto seed = static_cast<std::uint64_t>(command.value);
        sim_.powerOn(seed);
        for (auto& watched : watched_) {
            if (watched) {
                sim_.record(watched->net);
                watched->pushedUntil = -1;
            }
        }
        pushNotice({NoticeKind::PowerOn}, stop);
        seed_.store(seed);
        simTime_.store(0);
        state_.store(RunState::Running);
        rebase();
        break;
    }
    case CommandKind::PowerOff:
        if (state_.load() != RunState::Off) {
            pushNotice({NoticeKind::PowerOff}, stop);
            state_.store(RunState::Off);
        }
        break;
    case CommandKind::Pause:
        if (state_.load() == RunState::Running) {
            state_.store(RunState::Paused);
        }
        break;
    case CommandKind::Resume:
        if (state_.load() == RunState::Paused) {
            state_.store(RunState::Running);
            rebase();
        }
        break;
    case CommandKind::SetSpeed:
        speed_ = static_cast<Speed>(command.value);
        rebase();
        break;
    case CommandKind::PauseAt:
        pauseAt_ = command.value;
        break;
    case CommandKind::ResumeUntil:
        pauseAt_ = command.value;
        if (state_.load() == RunState::Paused) {
            state_.store(RunState::Running);
            rebase();
        }
        break;
    case CommandKind::Watch: {
        for (std::size_t slot = 0; slot < kMaxWatched; ++slot) {
            auto& watched = watched_[slot];
            if (watched && watched->net == command.net) {
                ++watched->count;
                pushNotice({NoticeKind::Rewatch, static_cast<std::uint8_t>(slot), command.net},
                           stop);
                return;
            }
        }
        std::size_t slot = 0;
        while (slot < kMaxWatched && (watched_[slot] || !slotFree_[slot].load())) {
            ++slot;
        }
        Check(slot < kMaxWatched, "runner: too many watched nets");
        slotFree_[slot].store(false);
        sim_.record(command.net);
        watched_[slot] = Watched{command.net, 1, -1};
        pushNotice({NoticeKind::Watch, static_cast<std::uint8_t>(slot), command.net}, stop);
        break;
    }
    case CommandKind::Unwatch:
        for (std::size_t slot = 0; slot < kMaxWatched; ++slot) {
            auto& watched = watched_[slot];
            if (watched && watched->net == command.net && --watched->count == 0) {
                pushNotice({NoticeKind::Unwatch, static_cast<std::uint8_t>(slot)}, stop);
                sim_.stopRecording(command.net);
                watched.reset();
            }
        }
        break;
    case CommandKind::Wake:
        if (state_.load() != RunState::Off) {
            sim_.wake(command.component, command.value);
        }
        break;
    case CommandKind::Post:
        command.action();
        if (command.wakes && state_.load() != RunState::Off) {
            sim_.wake(command.component, command.value);
        }
        break;
    }
}

void Runner::runSlice(std::stop_token stop) {
    const auto start = Clock::now();
    sim_.runUntil(sim_.now() + kSlice);
    for (std::size_t slot = 0; slot < kMaxWatched; ++slot) {
        auto& watched = watched_[slot];
        if (!watched) {
            continue;
        }
        for (const Sample sample : sim_.recorder().samples(watched->net)) {
            if (sample.time > watched->pushedUntil && !push(*rings_[slot], sample, stop)) {
                return;
            }
        }
        watched->pushedUntil = sim_.now();
    }
    pushNotice({NoticeKind::SliceEnd, 0, NetId{}, sim_.now()}, stop);
    sim_.trimRecordings();
    simTime_.store(sim_.now());
    runNs_.fetch_add(NsSince(start));
    if (pauseAt_ && sim_.now() >= *pauseAt_) {
        state_.store(RunState::Paused);
        pauseAt_.reset();
    }
}

template <class T>
bool Runner::push(SpscRing<T>& ring, const T& value, std::stop_token stop) {
    while (!ring.tryPush(value)) {
        if (stop.stop_requested()) {
            return false;
        }
        const auto start = Clock::now();
        kickDevices();
        std::this_thread::sleep_for(kFullRingWait);
        waitedNs_.fetch_add(NsSince(start));
    }
    return true;
}

void Runner::pushNotice(const Notice& notice, std::stop_token stop) {
    if (push(notices_, notice, stop)) {
        noticesPushed_.fetch_add(1);
        kickDevices();
    }
}

void Runner::kickDevices() {
    devicesWake_.fetch_add(1);
    devicesWake_.notify_one();
}

// --- devices thread ---

void Runner::drainRings() {
    for (std::size_t slot = 0; slot < kMaxWatched; ++slot) {
        if (!dispatcher_.isActive(static_cast<std::uint8_t>(slot))) {
            continue;
        }
        while (const auto sample = rings_[slot]->tryPop()) {
            dispatcher_.stage(static_cast<std::uint8_t>(slot), *sample);
        }
    }
}

void Runner::serveDevices(std::stop_token stop) {
    while (true) {
        const std::uint64_t seen = devicesWake_.load();
        if (stop.stop_requested()) {
            return;
        }
        drainRings();
        const auto notice = notices_.tryPop();
        if (!notice) {
            devicesWake_.wait(seen);
            continue;
        }
        drainRings();
        dispatcher_.apply(*notice);
        if (notice->kind == NoticeKind::Unwatch) {
            slotFree_[notice->slot].store(true);
        }
        noticesDone_.fetch_add(1);
        noticesDone_.notify_all();
    }
}

}  // namespace bw
