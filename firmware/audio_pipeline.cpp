#include "audio_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include <Arduino.h>
AudioStep AudioPipeline::process(KeywordEvent& event) {
  uint32_t start = micros();
  AudioStep result{false, false, false};
  auto audio = AudioCapture::snapshot();
  if (audio.droppedSamples != seenDrops) {
    seenDrops = audio.droppedSamples;
    AudioCapture::discardPending();
    Features::reset();
    result.reset = true;
  }
  uint32_t captureEndUs;
  if (!AudioCapture::readHop(audioHop, captureEndUs)) return result;
  result.processed = true;
  uint32_t dspStart = micros();
  Features::consume(audioHop);
  metrics.dspTiming.record(micros()-dspStart);
  if (Features::inferenceDue() && AudioCapture::available() < HOP_SIZE) {
    Features::markInferred();
    uint32_t inferenceStart = micros();
    bool success = KeywordModel::predict(Features::data(), metrics.latestProbability);
    metrics.inferenceTiming.record(micros()-inferenceStart);
    if (!success) { ++metrics.inferenceErrors; result.reset = true; }
    else {
      event = {metrics.latestProbability, captureEndUs, micros(), millis(), 0, 0};
      result.hasEvent = true;
    }
  }
  metrics.processingTiming.record(micros()-start);
  return result;
}
