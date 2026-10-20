#pragma once
#include "pipeline_metrics.h"
#include <cstdint>
#include "kws_config.h"
#include "keyword_event.h"
class AudioCapture;
class FeatureExtractor;
class KeywordModel;
struct AudioStep {
    bool processed, hasEvent, reset;
};
// Called only by the audio task. Model/feature buffers never leave this owner.
class AudioPipeline {
    AudioCapture &capture;
    FeatureExtractor &extractor;
    KeywordModel &model;
    PipelineMetrics metrics;
    std::uint32_t seenDrops = 0;
    AudioHop audioHop{};

public:
    AudioPipeline(AudioCapture &capture, FeatureExtractor &extractor, KeywordModel &model)
        : capture(capture), extractor(extractor), model(model) {}
    AudioPipeline(const AudioPipeline &) = delete;
    AudioPipeline &operator=(const AudioPipeline &) = delete;

    AudioStep process(KeywordEvent &event);
    const PipelineMetrics &statistics() const {
        return metrics;
    }
};
