#include "kws_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "keyword_detector.h"
#include "device_control.h"
#include "telemetry.h"
#include <Arduino.h>
namespace KwsPipeline {
namespace {
PipelineMetrics metrics;
KeywordDetector detector;
uint32_t seenDrops = 0;
int16_t audioHop[HOP_SIZE];
}
bool begin() {
  DeviceControl::begin();
  Telemetry::begin();
  if (!KeywordModel::begin()) { Serial.println("Model initialization failed"); return false; }
  if (!Features::begin(KeywordModel::inputScale(), KeywordModel::inputZeroPoint())) {
    Serial.println("DSP initialization failed"); return false;
  }
  if (!AudioCapture::begin()) { Serial.println("PDM failed"); return false; }
  Serial.println("Setup complete.");
  return true;
}
void poll() {
  uint32_t processingStart = micros();
  Telemetry::pump();
  auto audio = AudioCapture::snapshot();
  if (audio.droppedSamples != seenDrops) {
    seenDrops = audio.droppedSamples;
    AudioCapture::discardPending();
    Features::reset();
    detector.reset();
  }
  uint32_t captureEndUs;
  if (AudioCapture::readHop(audioHop, captureEndUs)) {
    uint32_t dspStart = micros();
    Features::consume(audioHop);
    metrics.dspTiming.record(micros() - dspStart);
    if (Features::inferenceDue() && AudioCapture::available() < HOP_SIZE) {
      Features::markInferred();
      uint32_t inferenceStart = micros();
      bool success = KeywordModel::predict(Features::data(), metrics.latestProbability);
      metrics.inferenceTiming.record(micros() - inferenceStart);
      if (!success) { ++metrics.inferenceErrors; detector.reset(); }
      else {
        auto detection = detector.update(metrics.latestProbability, captureEndUs, micros(), millis());
        if (detection.confirmed) {
          DeviceControl::applyKeywordAction();
          ++metrics.confirmedKeywords;
          if (detection.hasResponse) metrics.responseTiming.record(detection.responseUs);
        }
      }
    }
    metrics.processingTiming.record(micros() - processingStart);
  }
  Telemetry::report(AudioCapture::snapshot(), metrics, DeviceControl::isOn());
}
}
