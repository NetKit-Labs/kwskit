#include "audio_capture.h"
#include <cstdint>
#include <Arduino.h>
#include <PDM.h>
#include "interrupt_lock.h"
#include <algorithm>

AudioCapture *AudioCapture::activeReceiver = nullptr;

void AudioCapture::receiveCallback() {
    if (activeReceiver) {
        activeReceiver->receive();
    }
}

void AudioCapture::receive() {
    std::uint32_t now = micros();
    if (lastAudioUs) {
        std::uint32_t gap = now - lastAudioUs;
        if (gap > callbackGapMaxUs)
            callbackGapMaxUs = gap;
    }
    lastAudioUs = now;
    bool missed = false;
    while (PDM.available() > 0) {
        const int bytes =
            std::min(PDM.available(), static_cast<int>(pdmBuffer.size() * sizeof(pdmBuffer[0])));
        const int received = PDM.read(pdmBuffer.data(), bytes);
        if (received <= 0)
            break;
        receivedSamples += received / 2;
        for (int i = 0; i < received / 2; ++i)
            if (!audioQueue.push(pdmBuffer[i]))
                missed = true;
    }
    if (missed)
        ++missedBuffers;
    std::uint32_t depth = audioQueue.available();
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
    PDM.setBufferSize(pdmBuffer.size() * sizeof(pdmBuffer[0]));
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

std::uint32_t AudioCapture::available() const {
    return audioQueue.available();
}
AudioCapture::Snapshot AudioCapture::snapshot() const {
    const InterruptLock lock;
    return Snapshot{receivedSamples, audioQueue.droppedSamples(), missedBuffers, queueHighWater,
        callbackGapMaxUs, lastAudioUs};
}
bool AudioCapture::readHop(AudioHop &samples, std::uint32_t &captureEndUs) {
    if (!audioQueue.pop(samples))
        return false;
    std::uint32_t arrival, backlog;
    {
        const InterruptLock lock;
        arrival = lastAudioUs;
        backlog = audioQueue.available();
    }
    captureEndUs = arrival - (static_cast<std::uint64_t>(backlog) * 1000000ULL) / SAMPLE_RATE;
    return true;
}
void AudioCapture::discardPending() {
    audioQueue.discardPending();
}
