#pragma once
#include "cpp_standard.h"
#include <cstdint>
struct TimingStats {
    std::uint32_t last = 0, maximum = 0, count = 0;
    std::uint64_t total = 0;
    void record(std::uint32_t elapsed) {
        last = elapsed;
        if (elapsed > maximum)
            maximum = elapsed;
        total += elapsed;
        ++count;
    }
    std::uint32_t average() const {
        return count ? static_cast<std::uint32_t>(total / count) : 0;
    }
};
