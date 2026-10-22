#include <cassert>
#include <limits>
#include <string>

#include <Arduino.h>

#include "telemetry.h"

SerialFake Serial;
std::uint32_t nowMs = 0;

std::uint32_t millis() {
    return nowMs;
}

std::uint32_t micros() {
    return nowMs * 1000;
}

void delay(unsigned long ms) {
    nowMs += ms;
}

void SerialFake::begin(unsigned long rate) {
    assert(rate == 115200);
}

std::size_t SerialFake::write(const std::uint8_t *data, std::size_t count) {
    assert(connected && count <= 64 && count <= static_cast<std::size_t>(space));
    ++writes;
    output.append(reinterpret_cast<const char *>(data), count);
    return count;
}

int main() {
    Telemetry telemetry;
    telemetry.begin();
    assert(nowMs == 2000); // Disconnected startup has a bounded monitor wait.
    AudioCapture::Snapshot audio{32000, 0, 0, 768, 16022, 1999000};
    PipelineMetrics metrics;
    telemetry.report(audio, metrics, false);
    Serial.connected = true;
    Serial.space = 128;
    telemetry.pump();
    assert(Serial.writes == 0); // The first report starts a post-startup interval.
    nowMs += 2000;
    telemetry.report(audio, metrics, false);
    Serial.connected = false;
    Serial.space = 128;
    telemetry.pump();
    assert(Serial.writes == 0); // Disconnection cannot block or consume the record.
    Serial.connected = true;
    Serial.space = 0;
    telemetry.pump();
    assert(Serial.writes == 0);
    Serial.space = 7;
    telemetry.pump();
    assert(Serial.output.size() == 7);
    Serial.space = 128;
    for (int i = 0; i < 20; ++i)
        telemetry.pump();
    assert(Serial.output.find("samples=32000") != std::string::npos);
    assert(Serial.output.find("stack_free=0/0\n") != std::string::npos);
    const auto first = Serial.output;
    telemetry.pump();
    assert(Serial.output == first); // A completed record is sent only once.
    nowMs += 2000;
    telemetry.report(audio, metrics, true);
    Serial.connected = false;
    telemetry.pump();
    assert(Serial.output == first);
    Serial.connected = true;
    for (int i = 0; i < 20; ++i)
        telemetry.pump();
    assert(Serial.output.find("led=1") != std::string::npos);
    for (const auto probability : {std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max(), -1.0f}) {
        nowMs += 2000;
        metrics.latestProbability = probability;
        Serial.output.clear();
        telemetry.report(audio, metrics, false);
        for (int i = 0; i < 20; ++i)
            telemetry.pump();
        assert(Serial.output.find("stack_free=0/0\n") != std::string::npos);
        assert(Serial.output.find("dsp_errors=0") != std::string::npos);
        assert(Serial.output.find("p_milli=1000") != std::string::npos ||
            Serial.output.find("p_milli=0") != std::string::npos);
    }
}
