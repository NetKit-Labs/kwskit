#include "../firmware/timing_stats.h"

#include <cassert>
#include <cstdint>

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
}
