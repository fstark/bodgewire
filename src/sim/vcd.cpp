#include "sim/vcd.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace bw {

namespace {

// Base-94 identifier over the printable characters '!'..'~'.
std::string VcdId(std::size_t index) {
    std::string id;
    do {
        id += static_cast<char>('!' + index % 94);
        index /= 94;
    } while (index > 0);
    return id;
}

std::string SanitizeName(std::string_view name) {
    std::string result(name);
    for (char& c : result) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '.';
        if (!ok) {
            c = '_';
        }
    }
    return result;
}

struct Change {
    Time time;
    std::size_t slot;
    LogicValue value;
};

}  // namespace

void WriteVcd(std::ostream& out, const Circuit& circuit, const Recorder& recorder, Time end) {
    const auto nets = recorder.nets();
    std::vector<std::string> ids;
    out << "$timescale 1ps $end\n";
    out << "$scope module bodgewire $end\n";
    for (std::size_t slot = 0; slot < nets.size(); ++slot) {
        ids.push_back(VcdId(slot));
        out << "$var wire 1 " << ids.back() << ' ' << SanitizeName(circuit.netName(nets[slot]))
            << " $end\n";
    }
    out << "$upscope $end\n";
    out << "$enddefinitions $end\n";

    std::vector<Change> changes;
    for (std::size_t slot = 0; slot < nets.size(); ++slot) {
        for (const Sample& sample : recorder.samples(nets[slot])) {
            changes.push_back({sample.time, slot, sample.value});
        }
    }
    // Stable: samples of one Net at the same time keep their order, so the last one wins.
    std::stable_sort(changes.begin(), changes.end(), [](const Change& a, const Change& b) {
        return a.time != b.time ? a.time < b.time : a.slot < b.slot;
    });

    bool first = true;
    Time current = 0;
    for (const Change& change : changes) {
        if (first || change.time != current) {
            current = change.time;
            first = false;
            out << '#' << current << '\n';
        }
        out << ToChar(change.value) << ids[change.slot] << '\n';
    }
    out << '#' << end << '\n';
}

}  // namespace bw
