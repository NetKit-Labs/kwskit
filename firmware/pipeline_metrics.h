#pragma once
#include "timing_stats.h"
struct PipelineMetrics {
  TimingStats dspTiming, inferenceTiming, processingTiming, responseTiming;
  uint32_t confirmedKeywords = 0, inferenceErrors = 0;
  TimingStats eventLatency;
  uint32_t eventDrops = 0, staleEvents = 0, notifyErrors = 0;
  uint32_t audioStackFree = 0, controlStackFree = 0;
  float latestProbability = 0;
};
