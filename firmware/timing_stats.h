#pragma once
#include <stdint.h>
struct TimingStats {
    uint32_t last = 0, maximum = 0, count = 0;
    uint64_t total = 0;
    void record(uint32_t elapsed) {
        last = elapsed;
        if (elapsed > maximum)
            maximum = elapsed;
        total += elapsed;
        ++count;
    }
    uint32_t average() const {
        return count ? uint32_t(total / count) : 0;
    }
};
