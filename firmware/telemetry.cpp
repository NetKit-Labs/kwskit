#include "telemetry.h"
#include <Arduino.h>
#include <USB/PluggableUSBSerial.h>
namespace Telemetry {
namespace {
char telemetry[512];
size_t telemetryLength = 0, telemetryOffset = 0;
void pumpInternal() {
  if (telemetryOffset >= telemetryLength || !_SerialUSB.connected()) return;
  uint32_t sent = 0;
  size_t remaining = telemetryLength - telemetryOffset;
  _SerialUSB.send_nb((uint8_t*)telemetry + telemetryOffset,
                 min(remaining, size_t(64)), &sent);
  telemetryOffset += sent;
}

}
void begin() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 2000) {}
}
void pump() { pumpInternal(); }
void report(const AudioCapture::Snapshot& audio, const PipelineMetrics& metrics, bool deviceOn) {
  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 2000 && telemetryOffset >= telemetryLength) {
    lastReport = millis();
    uint32_t samples = audio.receivedSamples, missed = audio.missedBuffers;
    uint32_t high = audio.highWater, gap = audio.gapMaxUs, arrival = audio.lastAudioUs;
    uint32_t dropped = audio.droppedSamples;
    int length = snprintf(telemetry, sizeof(telemetry),
      "ms=%lu samples=%lu dropped=%lu missed_buffers=%lu queue_max=%lu gap_max_us=%lu audio_age_us=%lu "
      "dsp_us=%lu/%lu infer_us=%lu/%lu process_us=%lu/%lu response_us=%lu/%lu "
      "inferences=%lu errors=%lu p_milli=%d triggers=%lu led=%d\n",
      (unsigned long)millis(), (unsigned long)samples, (unsigned long)dropped,
      (unsigned long)missed, (unsigned long)high, (unsigned long)gap,
      (unsigned long)(micros()-arrival),
      (unsigned long)metrics.dspTiming.average(), (unsigned long)metrics.dspTiming.maximum,
      (unsigned long)metrics.inferenceTiming.average(), (unsigned long)metrics.inferenceTiming.maximum,
      (unsigned long)metrics.processingTiming.average(), (unsigned long)metrics.processingTiming.maximum,
      (unsigned long)metrics.responseTiming.last, (unsigned long)metrics.responseTiming.maximum,
      (unsigned long)metrics.inferenceTiming.count, (unsigned long)metrics.inferenceErrors,
      int(metrics.latestProbability*1000), (unsigned long)metrics.confirmedKeywords, int(deviceOn));
    telemetryLength = length > 0 ? min(size_t(length), sizeof(telemetry)-1) : 0;
    telemetryOffset = 0;
  }
}
}
