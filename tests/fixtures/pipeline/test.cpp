#include <cassert>
#include <cstdint>

#include <Arduino.h>

#include "audio_capture.h"
#include "audio_pipeline.h"
#include "feature_extractor.h"
#include "keyword_event.h"
#include "keyword_model.h"
std::uint32_t clockUs = 0;

std::uint32_t micros() {
    return clockUs;
}

std::uint32_t millis() {
    return clockUs / 1000;
}

bool predictOk = true, hop = true, due = true;
int queued = 0, predictions = 0, resets = 0, discards = 0;
AudioCapture::Snapshot audio{};

bool AudioCapture::readHop(AudioHop &, std::uint32_t &time) {
    (void)notificationContext;
    (void)notifyConsumer;
    (void)running;
    (void)pdmBuffer;
    (void)receivedSamples;
    (void)missedBuffers;
    (void)lastAudioUs;
    (void)queueHighWater;
    (void)callbackGapMaxUs;
    time = clockUs;
    return hop;
}

std::uint32_t AudioCapture::available() const {
    return queued;
}

AudioCapture::Snapshot AudioCapture::snapshot() const {
    return audio;
}

void AudioCapture::end() {}

void AudioCapture::discardPending() {
    ++discards;
}

void FeatureExtractor::reset() {
    (void)inputScale;
    (void)inputZeroPoint;
    (void)frameCount;
    (void)framesSinceInference;
    (void)firstFrame;
    ++resets;
}

void FeatureExtractor::consume(const AudioHop &) {
    clockUs += 2000;
}

bool FeatureExtractor::inferenceDue() const {
    return due;
}

void FeatureExtractor::markInferred() {}

const FeatureTensor &FeatureExtractor::features() const {
    return featureBuffer;
}

bool KeywordModel::predict(const FeatureTensor &, float &p) {
    (void)runtime;
    ++predictions;
    clockUs += 59000;
    p = 1;
    return predictOk;
}

struct KeywordModel::Runtime {};

KeywordModel &KeywordModel::instance() {
    static Runtime runtime;
    static KeywordModel model(runtime);
    return model;
}

int main() {
    AudioCapture capture;
    FeatureExtractor features;
    AudioPipeline pipeline(capture, features, KeywordModel::instance());
    KeywordEvent event{};
    hop = false;
    auto step = pipeline.process(event);
    assert(!step.processed && !step.hasEvent);
    hop = true;
    due = false;
    step = pipeline.process(event);
    assert(step.processed && !step.hasEvent);
    due = true;
    queued = HOP_SIZE;
    step = pipeline.process(event);
    assert(!step.hasEvent && predictions == 0);
    queued = 0;
    step = pipeline.process(event);
    assert(step.hasEvent && predictions == 1);
    assert(event.probability == 1 && event.decisionUs == clockUs &&
        event.captureEndUs == clockUs - 61000);
    assert(pipeline.statistics().dspTiming.maximum == 2000);
    assert(pipeline.statistics().inferenceTiming.maximum == 59000);
    predictOk = false;
    step = pipeline.process(event);
    assert(step.reset && !step.hasEvent && pipeline.statistics().inferenceErrors == 1);
    predictOk = true;
    audio.droppedSamples = 1;
    step = pipeline.process(event);
    assert(step.reset && discards == 1 && resets == 1);
    step = pipeline.process(event);
    assert(!step.reset && discards == 1);
}
