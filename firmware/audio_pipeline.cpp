#include "audio_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include <Arduino.h>
AudioStep AudioPipeline::process(KeywordEvent &event) {
    uint32_t start = micros();
    AudioStep result{false, false, false};
    auto audio = capture.snapshot();
    if (audio.droppedSamples != seenDrops) {
        seenDrops = audio.droppedSamples;
        capture.discardPending();
        features.reset();
        result.reset = true;
    }
    uint32_t captureEndUs;
    if (!capture.readHop(audioHop, captureEndUs))
        return result;
    result.processed = true;
    uint32_t dspStart = micros();
    features.consume(audioHop);
    metrics.dspTiming.record(micros() - dspStart);
    if (features.inferenceDue() && capture.available() < HOP_SIZE) {
        features.markInferred();
        uint32_t inferenceStart = micros();
        bool success = model.predict(features.data(), metrics.latestProbability);
        metrics.inferenceTiming.record(micros() - inferenceStart);
        if (!success) {
            ++metrics.inferenceErrors;
            result.reset = true;
        } else {
            event = {metrics.latestProbability, captureEndUs, micros(), millis(), 0, 0};
            result.hasEvent = true;
        }
    }
    metrics.processingTiming.record(micros() - start);
    return result;
}
