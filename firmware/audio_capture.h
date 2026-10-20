#pragma once
#include "kws_config.h"
#include <cstdint>
#include "audio_queue.h"
#include <array>

// The PDM driver supports one receiver. Its trampoline dispatches to the active owner.
class AudioCapture {
public:
    struct Snapshot {
        std::uint32_t receivedSamples, droppedSamples, missedBuffers, highWater, gapMaxUs,
            lastAudioUs;
    };
    using Notification = void (*)(void *);

    AudioCapture() = default;
    ~AudioCapture() {
        end();
    }
    AudioCapture(const AudioCapture &) = delete;
    AudioCapture &operator=(const AudioCapture &) = delete;
    // This object must outlive capture. The application gives it static lifetime.
    bool begin(Notification notify = nullptr, void *context = nullptr);
    void end();
    bool readHop(AudioHop &samples, std::uint32_t &captureEndUs);
    std::uint32_t available() const;
    Snapshot snapshot() const;
    void discardPending();

private:
    static void receiveCallback();
    void receive();
    static AudioCapture *activeReceiver;
    Notification notifyConsumer = nullptr;
    void *notificationContext = nullptr;
    bool running = false;
    std::array<std::int16_t, PDM_BUFFER_SIZE> pdmBuffer{};
    AudioQueue<8192> audioQueue;
    volatile std::uint32_t receivedSamples = 0, missedBuffers = 0, lastAudioUs = 0;
    volatile std::uint32_t queueHighWater = 0, callbackGapMaxUs = 0;
};
