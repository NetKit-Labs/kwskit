#include "telemetry.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <Arduino.h>
#include <USB/PluggableUSBSerial.h>

void Telemetry::pump() {
    if (offset >= length || !_SerialUSB.connected())
        return;

    std::uint32_t sent = 0;
    std::size_t remaining = length - offset;
    _SerialUSB.send_nb(reinterpret_cast<std::uint8_t *>(buffer.data()) + offset,
        std::min(remaining, std::size_t{64}), &sent);
    offset += sent;
}

void Telemetry::begin() {
    Serial.begin(115200);
    std::uint32_t start = millis();

    while (!Serial && millis() - start < 2000) {
    }
}

void Telemetry::report(
    const AudioCapture::Snapshot &audio, const PipelineMetrics &metrics, bool deviceOn) {
    if (millis() - lastReport >= 2000 && offset >= length) {
        lastReport = millis();
        std::uint32_t samples = audio.receivedSamples, missed = audio.missedBuffers;
        std::uint32_t high = audio.highWater, gap = audio.gapMaxUs, arrival = audio.lastAudioUs;
        std::uint32_t dropped = audio.droppedSamples;

        const int formattedLength = std::snprintf(buffer.data(), buffer.size(),
            "ms=%lu samples=%lu dropped=%lu missed_buffers=%lu queue_max=%lu gap_max_us=%lu "
            "audio_age_us=%lu "
            "dsp_us=%lu/%lu infer_us=%lu/%lu process_us=%lu/%lu response_us=%lu/%lu "
            "inferences=%lu errors=%lu p_milli=%d triggers=%lu led=%d event_drops=%lu "
            "stale_events=%lu notify_errors=%lu event_us=%lu/%lu stack_free=%lu/%lu\n",
            static_cast<unsigned long>(millis()), static_cast<unsigned long>(samples),
            static_cast<unsigned long>(dropped), static_cast<unsigned long>(missed),
            static_cast<unsigned long>(high), static_cast<unsigned long>(gap),
            static_cast<unsigned long>(micros() - arrival),
            static_cast<unsigned long>(metrics.dspTiming.average()),
            static_cast<unsigned long>(metrics.dspTiming.maximum),
            static_cast<unsigned long>(metrics.inferenceTiming.average()),
            static_cast<unsigned long>(metrics.inferenceTiming.maximum),
            static_cast<unsigned long>(metrics.processingTiming.average()),
            static_cast<unsigned long>(metrics.processingTiming.maximum),
            static_cast<unsigned long>(metrics.responseTiming.last),
            static_cast<unsigned long>(metrics.responseTiming.maximum),
            static_cast<unsigned long>(metrics.inferenceTiming.count),
            static_cast<unsigned long>(metrics.inferenceErrors),
            static_cast<int>(metrics.latestProbability * 1000),
            static_cast<unsigned long>(metrics.confirmedKeywords), static_cast<int>(deviceOn),
            static_cast<unsigned long>(metrics.eventDrops),
            static_cast<unsigned long>(metrics.staleEvents),
            static_cast<unsigned long>(metrics.notifyErrors),
            static_cast<unsigned long>(metrics.eventLatency.average()),
            static_cast<unsigned long>(metrics.eventLatency.maximum),
            static_cast<unsigned long>(metrics.audioStackFree),
            static_cast<unsigned long>(metrics.controlStackFree));

        length = formattedLength > 0
            ? std::min(static_cast<std::size_t>(formattedLength), buffer.size() - 1)
            : 0;
        offset = 0;
    }
}
