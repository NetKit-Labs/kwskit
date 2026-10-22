#include "audio_capture.h"

#include <algorithm>
#include <cstdint>

#include <Arduino.h>
#include <PDM.h>

#include "interrupt_lock.h"

constinit std::atomic<AudioCapture *> AudioCapture::activeReceiver{nullptr};

void AudioCapture::receiveCallback() {
    if (auto *receiver = activeReceiver.load(std::memory_order_acquire))
        receiver->receive();
}

void AudioCapture::receive() {
    std::uint32_t now = micros();

    const auto previousArrival = lastAudioUs.load(std::memory_order_relaxed);
    if (previousArrival) {
        std::uint32_t gap = now - previousArrival;
        if (gap > callbackGapMaxUs.load(std::memory_order_relaxed))
            callbackGapMaxUs.store(gap, std::memory_order_relaxed);
    }
    lastAudioUs.store(now, std::memory_order_relaxed);

    bool missed = false;

    // The driver publishes one configured PCM block per callback. Even a faulty
    // driver that keeps reporting data cannot extend this callback beyond one block.
    int remaining = sizeof(pdmBuffer);
    while (remaining >= static_cast<int>(sizeof(pdmBuffer[0]))) {
        const int bytes = std::min(PDM.available(), remaining) & ~1;
        if (bytes <= 0)
            break;
        const int received = PDM.read(pdmBuffer.data(), bytes);
        if (received <= 0 || received > bytes || received % sizeof(pdmBuffer[0]) != 0) {
            missed = true;
            break;
        }
        remaining -= received;
        receivedSamples.store(receivedSamples.load(std::memory_order_relaxed) + received / 2,
            std::memory_order_relaxed);
        for (int i = 0; i < received / 2; ++i)
            if (!audioQueue.push(pdmBuffer[i]))
                missed = true;
    }

    if (missed)
        missedBuffers.store(
            missedBuffers.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    std::uint32_t depth = audioQueue.available();
    if (depth > queueHighWater.load(std::memory_order_relaxed))
        queueHighWater.store(depth, std::memory_order_relaxed);

    if (notifyConsumer)
        notifyConsumer(notificationContext);
}

bool AudioCapture::begin(Notification notify, void *context) {
    if (activeReceiver.load(std::memory_order_acquire)) {
        return false;
    }

    notifyConsumer = notify;
    notificationContext = context;
    activeReceiver.store(this, std::memory_order_release);
    PDM.onReceive(receiveCallback);
    if (!PDM.begin(1, SAMPLE_RATE)) {
        activeReceiver.store(nullptr, std::memory_order_release);
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
    if (activeReceiver.load(std::memory_order_acquire) == this) {
        activeReceiver.store(nullptr, std::memory_order_release);
    }

    notifyConsumer = nullptr;
    notificationContext = nullptr;
}

std::uint32_t AudioCapture::available() const {
    return audioQueue.available();
}

AudioCapture::Snapshot AudioCapture::snapshot() const {
    const InterruptLock lock;
    return Snapshot{
        .receivedSamples = receivedSamples.load(std::memory_order_relaxed),
        .droppedSamples = audioQueue.droppedSamples(),
        .missedBuffers = missedBuffers.load(std::memory_order_relaxed),
        .highWater = queueHighWater.load(std::memory_order_relaxed),
        .gapMaxUs = callbackGapMaxUs.load(std::memory_order_relaxed),
        .lastAudioUs = lastAudioUs.load(std::memory_order_relaxed),
    };
}

bool AudioCapture::readHop(AudioHop &samples, std::uint32_t &captureEndUs) {
    if (!audioQueue.pop(samples))
        return false;

    std::uint32_t arrival, backlog;
    {
        const InterruptLock lock;
        arrival = lastAudioUs.load(std::memory_order_relaxed);
        backlog = audioQueue.available();
    }

    captureEndUs = arrival - (static_cast<std::uint64_t>(backlog) * 1000000ULL) / SAMPLE_RATE;
    return true;
}

void AudioCapture::discardPending() {
    audioQueue.discardPending();
}
