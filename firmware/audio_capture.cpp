#include "audio_capture.h"
#include <Arduino.h>
#include <PDM.h>

AudioCapture *AudioCapture::activeReceiver = nullptr;

void AudioCapture::receiveCallback() {
    if (activeReceiver) {
        activeReceiver->receive();
    }
}

void AudioCapture::receive() {
    uint32_t now = micros();
    if (lastAudioUs) {
        uint32_t gap = now - lastAudioUs;
        if (gap > callbackGapMaxUs)
            callbackGapMaxUs = gap;
    }
    lastAudioUs = now;
    bool missed = false;
    while (PDM.available() > 0) {
        int bytes = min(PDM.available(), static_cast<int>(sizeof(pdmBuffer)));
        int received = PDM.read(pdmBuffer.data(), bytes);
        if (received <= 0)
            break;
        receivedSamples += received / 2;
        for (int i = 0; i < received / 2; ++i)
            if (!audioQueue.push(pdmBuffer[i]))
                missed = true;
    }
    if (missed)
        ++missedBuffers;
    uint32_t depth = audioQueue.available();
    if (depth > queueHighWater)
        queueHighWater = depth;
    if (notifyConsumer)
        notifyConsumer(notificationContext);
}

bool AudioCapture::begin(Notification notify, void *context) {
    if (activeReceiver) {
        return false;
    }
    notifyConsumer = notify;
    notificationContext = context;
    activeReceiver = this;
    PDM.onReceive(receiveCallback);
    PDM.setBufferSize(sizeof(pdmBuffer));
    if (!PDM.begin(1, SAMPLE_RATE)) {
        activeReceiver = nullptr;
        notifyConsumer = nullptr;
        notificationContext = nullptr;
        return false;
    }
    running = true;
    PDM.setGain(60);
    return true;
}

void AudioCapture::end() {
    if (running) {
        PDM.end();
    }
    running = false;
    if (activeReceiver == this) {
        activeReceiver = nullptr;
    }
    notifyConsumer = nullptr;
    notificationContext = nullptr;
}

uint32_t AudioCapture::available() const {
    return audioQueue.available();
}
AudioCapture::Snapshot AudioCapture::snapshot() const {
    noInterrupts();
    Snapshot result{receivedSamples, audioQueue.droppedSamples, missedBuffers, queueHighWater,
        callbackGapMaxUs, lastAudioUs};
    interrupts();
    return result;
}
bool AudioCapture::readHop(int16_t *samples, uint32_t &captureEndUs) {
    if (!audioQueue.pop(samples, HOP_SIZE))
        return false;
    noInterrupts();
    uint32_t arrival = lastAudioUs, backlog = audioQueue.available();
    interrupts();
    captureEndUs = arrival - (static_cast<uint64_t>(backlog) * 1000000ULL) / SAMPLE_RATE;
    return true;
}
void AudioCapture::discardPending() {
    int16_t discarded[HOP_SIZE];
    while (audioQueue.pop(discarded, HOP_SIZE)) {
    }
    while (audioQueue.pop(discarded, 1)) {
    }
}
