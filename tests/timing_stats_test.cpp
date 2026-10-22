#include "../firmware/timing_stats.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    TimingStats s;
    assert(s.average() == 0);
    s.record(10);
    s.record(30);
    s.record(20);
    assert(s.last == 20 && s.maximum == 30 && s.average() == 20 && s.count == 3);
    std::uint32_t start = 0xfffffff0u, end = 0x10u;
    s.record(end - start);
    assert(s.last == 32 && s.maximum == 32);
    TimingStats longRunning;
    constexpr auto max = std::numeric_limits<std::uint32_t>::max();
    longRunning.count = longRunning.averagedCount = max;
    longRunning.total = static_cast<std::uint64_t>(max) * 10;
    longRunning.record(10);
    assert(longRunning.count == 0 && longRunning.average() == 10);
    longRunning.record(10);
    assert(longRunning.count == 1 && longRunning.average() == 10);
    longRunning.averagedCount = max;
    longRunning.total = static_cast<std::uint64_t>(max) * max;
    longRunning.record(max);
    assert(longRunning.average() == max && longRunning.maximum == max);
}
