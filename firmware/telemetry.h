#pragma once
#include "pipeline_metrics.h"
#include "audio_capture.h"
namespace Telemetry {
void begin();
void pump();
void report(const AudioCapture::Snapshot& audio, const PipelineMetrics& metrics, bool deviceOn);
}
