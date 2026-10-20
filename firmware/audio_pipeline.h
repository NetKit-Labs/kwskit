#pragma once
#include "pipeline_metrics.h"
#include "kws_config.h"
#include "keyword_event.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
struct AudioStep {
    bool processed, hasEvent, reset;
};
// Called only by the audio task. Model/feature buffers never leave this owner.
class AudioPipeline {
    AudioCapture &capture;
    FeatureExtractor &features;
    KeywordModel &model;
    PipelineMetrics metrics;
    uint32_t seenDrops = 0;
    int16_t audioHop[HOP_SIZE];

public:
    AudioPipeline(AudioCapture &capture, FeatureExtractor &features, KeywordModel &model)
        : capture(capture), features(features), model(model) {}
    AudioStep process(KeywordEvent &event);
    const PipelineMetrics &statistics() const {
        return metrics;
    }
};
