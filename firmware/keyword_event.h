#pragma once
#include "keyword_detector.h"
#include <type_traits>
#include <cstdint>
#include <cmath>

// CMSIS queue copies this value; no audio/feature/tensor pointers cross tasks.
struct KeywordEvent {
    float probability;
    std::uint32_t captureEndUs, decisionUs, decisionMs, sequence, generation;
};
static_assert(
    std::is_trivially_copyable_v<KeywordEvent>, "CMSIS copies event object representations");
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
    static bool isCurrent(const KeywordEvent &event, std::uint32_t generation, std::uint32_t nowUs,
        std::uint32_t nowMs) {
        // Both clocks prevent an event delayed for a full micros() rollover looking fresh.
        return std::isfinite(event.probability) && event.probability >= 0.0f &&
            event.probability <= 1.0f && event.generation == generation &&
            static_cast<std::uint32_t>(nowUs - event.decisionUs) <= maxAgeUs &&
            static_cast<std::uint32_t>(nowMs - event.decisionMs) <= maxAgeUs / 1000;
    }
    EventResult consume(const KeywordEvent &event, std::uint32_t currentGeneration,
        std::uint32_t nowUs, std::uint32_t nowMs) {
        if (!isCurrent(event, currentGeneration, nowUs, nowMs)) {
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
