#pragma once
#include "pipeline_metrics.h"
#include "audio_capture.h"
#include <array>
#include <cstddef>

// Used only by the control task after initialization.
class Telemetry {
public:
    void begin();
    void pump();
    void report(const AudioCapture::Snapshot &audio, const PipelineMetrics &metrics, bool deviceOn);

private:
    std::array<char, 640> buffer{};
    size_t length = 0;
    size_t offset = 0;
    uint32_t lastReport = 0;
};
