#pragma once

#include "kws_config.h"

#include <array>
#include <cstdint>

#include <Arduino.h>
#include <arm_math.h>

// One audio-task-owned context. CMSIS descriptors point into this object's buffers.
class FeatureExtractor {
public:
    FeatureExtractor() = default;
    FeatureExtractor(const FeatureExtractor &) = delete;
    FeatureExtractor &operator=(const FeatureExtractor &) = delete;

    bool begin(float scale, std::int32_t zeroPoint);
    void reset();
    void consume(const AudioHop &samples);
    bool inferenceDue() const;
    void markInferred();
    const FeatureTensor &features() const;

private:
    void computeHannWindow();
    void computeLogMel();

    float inputScale = 0.0f;
    std::int32_t inputZeroPoint = 0;

    int frameCount = 0;
    int framesSinceInference = 0;
    bool firstFrame = true;

    std::array<float, FRAME_SIZE> audioFrame{};
    std::array<float, FRAME_SIZE> windowed{};
    std::array<float, FRAME_SIZE> hannWindow{};
    std::array<float, FRAME_SIZE> fftOutput{};
    std::array<float, SPECTRUM_BINS> powerSpectrum{};
    std::array<float, NUM_MEL> melRow{};
    std::array<std::int8_t, NUM_MEL> quantizedMelRow{};
    FeatureTensor featureBuffer{};

    arm_matrix_instance_f32 melMatrix{}, spectrumVector{}, melVector{};
    arm_rfft_fast_instance_f32 fft{};
};
