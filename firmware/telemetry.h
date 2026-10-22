#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "audio_capture.h"
#include "pipeline_metrics.h"

// Best-effort output owned by main at priority 6; never called by critical workers.
class Telemetry {
public:
    Telemetry() = default;
    Telemetry(const Telemetry &) = delete;
    Telemetry &operator=(const Telemetry &) = delete;

    void begin();
    void pump();
    void report(const AudioCapture::Snapshot &audio, const PipelineMetrics &metrics, bool deviceOn);

private:
    std::array<char, 640> buffer{};
    std::size_t length = 0;
    std::size_t offset = 0;
    std::uint32_t lastReport = 0;
    bool reporting = false;
};
