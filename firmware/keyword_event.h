#pragma once
#include "keyword_detector.h"
#include <type_traits>
#include <cstdint>

// CMSIS queue copies this value; no audio/feature/tensor pointers cross tasks.
struct KeywordEvent {
    float probability;
    std::uint32_t captureEndUs, decisionUs, decisionMs, sequence, generation;
};
static_assert(
    std::is_trivially_copyable<KeywordEvent>::value, "CMSIS copies event object representations");
struct EventResult {
    DetectionResult detection;
    bool discarded;
};
class KeywordEventFilter {
    KeywordDetector detector;
    bool havePrevious = false;
    std::uint32_t previousSequence = 0, previousGeneration = 0, previousDecisionMs = 0;

public:
    static constexpr std::uint32_t maxAgeUs = 250000;
    EventResult consume(const KeywordEvent &event, std::uint32_t currentGeneration,
        std::uint32_t nowUs, std::uint32_t nowMs) {
        if (event.generation != currentGeneration ||
            static_cast<std::uint32_t>(nowUs - event.decisionUs) > maxAgeUs) {
            detector.resetEvidence();
            havePrevious = false;
            return {{false, false, 0}, true};
        }
        if (!havePrevious || event.generation != previousGeneration ||
            event.sequence != previousSequence + 1 ||
            static_cast<std::uint32_t>(event.decisionMs - previousDecisionMs) > 400)
            detector.resetEvidence();
        previousGeneration = event.generation;
        previousSequence = event.sequence;
        previousDecisionMs = event.decisionMs;
        havePrevious = true;
        return {detector.update(event.probability, event.captureEndUs, nowUs, nowMs), false};
    }
};
