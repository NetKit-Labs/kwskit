#pragma once
#include "pipeline_metrics.h"
#include "kws_config.h"
#include "keyword_event.h"
struct AudioStep { bool processed, hasEvent, reset; };
// Called only by the audio task. Model/feature buffers never leave this owner.
class AudioPipeline {
  PipelineMetrics metrics;
  uint32_t seenDrops = 0;
  int16_t audioHop[HOP_SIZE];
public:
  AudioStep process(KeywordEvent& event);
  const PipelineMetrics& statistics() const { return metrics; }
};
