#pragma once
#include "kws_config.h"
namespace AudioCapture {
struct Snapshot {
  uint32_t receivedSamples, droppedSamples, missedBuffers, highWater, gapMaxUs, lastAudioUs;
};
bool begin();
bool readHop(int16_t* samples, uint32_t& captureEndUs);
uint32_t available();
Snapshot snapshot();
void discardPending();
}
