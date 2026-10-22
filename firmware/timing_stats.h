#pragma once

#include "cpp_standard.h"

#include <cstdint>
#include <limits>

struct TimingStats {
    std::uint32_t last = 0, maximum = 0, count = 0;
    std::uint64_t total = 0;
    std::uint32_t averagedCount = 0;

    void record(std::uint32_t elapsed) noexcept {
        last = elapsed;
        if (elapsed > maximum)
            maximum = elapsed;
        // Rebase only the average window; the public event count wraps independently.
        if (averagedCount == std::numeric_limits<std::uint32_t>::max()) {
            total /= 2;
            averagedCount /= 2;
        }
        total += elapsed;
        ++averagedCount;
        ++count;
    }

    [[nodiscard]] std::uint32_t average() const noexcept {
        return averagedCount ? static_cast<std::uint32_t>(total / averagedCount) : 0;
    }
};
