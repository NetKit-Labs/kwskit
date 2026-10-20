#pragma once
#include "timing_stats.h"
struct PipelineMetrics {
  TimingStats dspTiming, inferenceTiming, processingTiming, responseTiming;
  uint32_t confirmedKeywords = 0, inferenceErrors = 0;
  float latestProbability = 0;
};
