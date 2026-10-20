#include "audio_capture.h"
#include "audio_queue.h"
#include <Arduino.h>
#include <PDM.h>
namespace AudioCapture {
namespace {
void (*notifyConsumer)() = nullptr;
bool running = false;
int16_t pdmBuffer[PDM_BUFFER_SIZE];
AudioQueue<8192> audioQueue;
volatile uint32_t receivedSamples = 0, missedBuffers = 0, lastAudioUs = 0;
volatile uint32_t queueHighWater = 0, callbackGapMaxUs = 0;
void onPDMData() {
  uint32_t now = micros();
  if (lastAudioUs) {
    uint32_t gap = now - lastAudioUs;
    if (gap > callbackGapMaxUs) callbackGapMaxUs = gap;
  }
  lastAudioUs = now;
  bool missed = false;
  while (PDM.available() > 0) {
    int bytes = min(PDM.available(), (int)sizeof(pdmBuffer));
    int received = PDM.read(pdmBuffer, bytes);
    if (received <= 0) break;
    receivedSamples += received / 2;
    for (int i = 0; i < received / 2; ++i)
      if (!audioQueue.push(pdmBuffer[i])) missed = true;
  }
  if (missed) ++missedBuffers;
  uint32_t depth = audioQueue.available();
  if (depth > queueHighWater) queueHighWater = depth;
  if (notifyConsumer) notifyConsumer();
}

}
bool begin(void (*notify)()) {
  notifyConsumer = notify;
  PDM.onReceive(onPDMData);
  PDM.setBufferSize(sizeof(pdmBuffer));
  if (!PDM.begin(1, SAMPLE_RATE)) { notifyConsumer = nullptr; return false; }
  running = true;
  PDM.setGain(60);
  return true;
}
void end() {
  if (running) PDM.end();
  running = false;
  notifyConsumer = nullptr;
}
uint32_t available() { return audioQueue.available(); }
Snapshot snapshot() {
  noInterrupts();
  Snapshot result{receivedSamples, audioQueue.droppedSamples, missedBuffers,
                  queueHighWater, callbackGapMaxUs, lastAudioUs};
  interrupts();
  return result;
}
bool readHop(int16_t* samples, uint32_t& captureEndUs) {
  if (!audioQueue.pop(samples, HOP_SIZE)) return false;
  noInterrupts();
  uint32_t arrival = lastAudioUs, backlog = audioQueue.available();
  interrupts();
  captureEndUs = arrival - (backlog * 1000000UL) / SAMPLE_RATE;
  return true;
}
void discardPending() {
  int16_t discarded[HOP_SIZE];
  while (audioQueue.pop(discarded, HOP_SIZE)) {}
  while (audioQueue.pop(discarded, 1)) {}
}
}
