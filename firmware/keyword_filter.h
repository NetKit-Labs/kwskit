#pragma once
#include "cpp_standard.h"
#include <cstdint>
class KeywordFilter {
    float smoothed = 0;
    std::uint8_t hits = 0;
    bool armed = true, triggered = false;
    std::uint32_t lastTrigger = 0;

public:
    // Discard temporal evidence without erasing cooldown or a latched detection.
    void resetEvidence() {
        hits = 0;
        // A latched detection retains its release EMA: a gap is not silence.
        if (armed)
            smoothed = 0;
    }
    // Scores arrive every ~128 ms. Require sustained evidence and release below 0.4.
    bool update(float probability, std::uint32_t now) {
        smoothed += 0.5f * (probability - smoothed);
        if (smoothed < 0.4f) {
            armed = true;
            hits = 0;
        }
        if (!armed)
            return false;
        if (smoothed >= 0.75f && probability >= 0.8f) {
            if (hits < 2)
                ++hits;
        } else
            hits = 0;
        if (hits < 2 || (triggered && static_cast<std::uint32_t>(now - lastTrigger) < 1500))
            return false;
        armed = false;
        triggered = true;
        lastTrigger = now;
        hits = 0;
        return true;
    }
};
