#include "feature_extractor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "fft_power.h"
#include "mel_filter_coefs.h"

void FeatureExtractor::computeHannWindow() {
    constexpr double pi = std::numbers::pi_v<double>;
    for (std::size_t n = 0; n < hannWindow.size(); ++n) {
        // Periodic Hann convention used by the training notebook.
        const auto angle = static_cast<float>(2.0f * pi * n / FRAME_SIZE);
        hannWindow[n] = 0.5f - 0.5f * std::cos(angle);
    }
}

void FeatureExtractor::computeLogMel() {
    arm_mult_f32(audioFrame.data(), hannWindow.data(), windowed.data(), FRAME_SIZE);
    arm_rfft_fast_f32(&fft, windowed.data(), fftOutput.data(), 0);
    fftPower(fftOutput, powerSpectrum);

    arm_mat_mult_f32(&melMatrix, &spectrumVector, &melVector);

    for (std::size_t m = 0; m < melRow.size(); ++m) {
        const float logMel = std::log(melRow[m] + 1e-6f);
        // Saturate in floating point before conversion, including overflow to infinity.
        const auto quantized =
            std::clamp(std::round(logMel / inputScale + inputZeroPoint), -128.0f, 127.0f);
        quantizedMelRow[m] = static_cast<std::int8_t>(quantized);
    }

    std::copy(featureBuffer.begin() + NUM_MEL, featureBuffer.end(), featureBuffer.begin());
    std::copy(quantizedMelRow.begin(), quantizedMelRow.end(), featureBuffer.end() - NUM_MEL);
}

void FeatureExtractor::consume(const AudioHop &samples) {
    if (firstFrame) {
        audioFrame.fill(0.0f);
        firstFrame = false;
        for (int i = 0; i < HOP_SIZE; ++i) {
            audioFrame[i + HOP_SIZE] = samples[i] / 32768.0f;
        }
        return;
    }

    std::copy(audioFrame.begin() + HOP_SIZE, audioFrame.end(), audioFrame.begin());
    for (int i = 0; i < HOP_SIZE; ++i) {
        audioFrame[i + HOP_SIZE] = samples[i] / 32768.0f;
    }

    computeLogMel();

    if (frameCount < NUM_FRAMES) {
        ++frameCount;
    }
    if (framesSinceInference < INFERENCE_HOP)
        ++framesSinceInference;
}

bool FeatureExtractor::begin(float scale, std::int32_t zeroPoint) {
    if (!std::isfinite(scale) || scale <= 0 || zeroPoint < -128 || zeroPoint > 127 ||
        arm_rfft_fast_init_f32(&fft, FRAME_SIZE) != ARM_MATH_SUCCESS) {
        return false;
    }

    inputScale = scale;
    inputZeroPoint = zeroPoint;

    // CMSIS takes a mutable pointer but matrix multiplication only reads these flash coefficients.
    arm_mat_init_f32(&melMatrix, NUM_MEL, SPECTRUM_BINS, const_cast<float *>(&melFilterbank[0][0]));
    arm_mat_init_f32(&spectrumVector, SPECTRUM_BINS, 1, powerSpectrum.data());
    arm_mat_init_f32(&melVector, NUM_MEL, 1, melRow.data());

    computeHannWindow();
    reset();
    return true;
}

void FeatureExtractor::reset() {
    firstFrame = true;
    frameCount = 0;
    framesSinceInference = 0;
}

bool FeatureExtractor::inferenceDue() const {
    return frameCount >= NUM_FRAMES && framesSinceInference >= INFERENCE_HOP;
}

void FeatureExtractor::markInferred() {
    framesSinceInference = 0;
}

const FeatureTensor &FeatureExtractor::features() const {
    return featureBuffer;
}
