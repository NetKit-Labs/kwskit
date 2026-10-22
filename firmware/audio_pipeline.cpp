#include "audio_pipeline.h"

#include <cstdint>

#include <Arduino.h>

#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"

AudioStep AudioPipeline::process(KeywordEvent &event) {
    std::uint32_t start = micros();
    AudioStep result{};

    const auto audio = capture.snapshot();
    if (audio.droppedSamples != seenDrops || audio.missedBuffers != seenMissedBuffers) {
        seenDrops = audio.droppedSamples;
        seenMissedBuffers = audio.missedBuffers;
        capture.discardPending();
        extractor.reset();
        result.reset = true;
    }

    std::uint32_t captureEndUs;
    if (!capture.readHop(audioHop, captureEndUs))
        return result;

    result.processed = true;
    std::uint32_t dspStart = micros();
    const bool featuresValid = extractor.consume(audioHop);
    metrics.dspTiming.record(micros() - dspStart);

    if (!featuresValid) {
        ++metrics.dspErrors;
        result.reset = true;
        metrics.processingTiming.record(micros() - start);
        return result;
    }

    if (extractor.inferenceDue() && capture.available() < HOP_SIZE) {
        extractor.markInferred();
        std::uint32_t inferenceStart = micros();
        bool success = model.predict(extractor.features(), metrics.latestProbability);
        metrics.inferenceTiming.record(micros() - inferenceStart);
        if (!success) {
            ++metrics.inferenceErrors;
            result.reset = true;
        } else {
            event = {
                .probability = metrics.latestProbability,
                .captureEndUs = captureEndUs,
                .decisionUs = micros(),
                .decisionMs = millis(),
                .sequence = 0,
                .generation = 0,
            };
            result.hasEvent = true;
        }
    }

    metrics.processingTiming.record(micros() - start);
    return result;
}
