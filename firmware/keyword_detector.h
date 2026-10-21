#pragma once

#include <cstdint>

#include "keyword_filter.h"

// Pure detection state: no GPIO, USB, microphone, or model dependencies.
struct DetectionResult {
    bool confirmed;
    bool hasResponse;
    std::uint32_t responseUs;
};

class KeywordDetector {
    KeywordFilter filter;
    std::uint32_t evidenceUs = 0;
    bool evidenceActive = false;

public:
    void reset() {
        filter = KeywordFilter{};
        evidenceActive = false;
    }

    void resetEvidence() {
        filter.resetEvidence();
        evidenceActive = false;
    }

    DetectionResult update(
        float probability, std::uint32_t captureEndUs, std::uint32_t nowUs, std::uint32_t nowMs) {
        if (probability >= 0.8f && !evidenceActive) {
            evidenceUs = captureEndUs;
            evidenceActive = true;
        } else if (probability < 0.4f)
            evidenceActive = false;
        bool confirmed = filter.update(probability, nowMs);
        return {
            confirmed, confirmed && evidenceActive, static_cast<std::uint32_t>(nowUs - evidenceUs)};
    }
};
