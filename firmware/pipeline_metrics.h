#pragma once
#include "timing_stats.h"
#include <cstdint>
struct PipelineMetrics {
    TimingStats dspTiming, inferenceTiming, processingTiming, responseTiming;
    std::uint32_t confirmedKeywords = 0, inferenceErrors = 0;
    TimingStats eventLatency;
    std::uint32_t eventDrops = 0, staleEvents = 0, notifyErrors = 0;
    std::uint32_t audioStackFree = 0, controlStackFree = 0;
    float latestProbability = 0;
};
