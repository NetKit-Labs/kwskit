#include "feature_extractor.h"
#include "fft_power.h"
#include "mel_filter_coefs.h"
#include <algorithm>
#include <cmath>

void FeatureExtractor::computeHannWindow() {
    constexpr double pi = 3.1415926535897932384626433832795;
    for (size_t n = 0; n < hannWindow.size(); ++n) {
        // Periodic Hann convention used by the training notebook.
        const auto angle = static_cast<float>(2.0f * pi * n / FRAME_SIZE);
        hannWindow[n] = 0.5f - 0.5f * std::cos(angle);
    }
}

void FeatureExtractor::computeLogMel() {
    arm_mult_f32(audioFrame.data(), hannWindow.data(), windowed.data(), FRAME_SIZE);
    arm_rfft_fast_f32(&fft, windowed.data(), fftOutput.data(), 0);
    fftPower(fftOutput.data(), powerSpectrum.data(), FRAME_SIZE);
    arm_mat_mult_f32(&melMatrix, &spectrumVector, &melVector);

    for (size_t m = 0; m < melRow.size(); ++m) {
        const float logMel = std::log(melRow[m] + 1e-6f);
        const auto quantized =
            static_cast<int32_t>(std::round(logMel / inputScale + inputZeroPoint));
        quantizedMelRow[m] =
            static_cast<int8_t>(std::max<int32_t>(-128, std::min<int32_t>(127, quantized)));
    }

    std::move(featureBuffer.begin() + NUM_MEL, featureBuffer.end(), featureBuffer.begin());
    std::copy(quantizedMelRow.begin(), quantizedMelRow.end(), featureBuffer.end() - NUM_MEL);
}

void FeatureExtractor::consume(const int16_t *samples) {
    if (firstFrame) {
        audioFrame.fill(0.0f);
        firstFrame = false;
        for (int i = 0; i < HOP_SIZE; ++i) {
            audioFrame[i + HOP_SIZE] = samples[i] / 32768.0f;
        }
        return;
    }

    std::move(audioFrame.begin() + HOP_SIZE, audioFrame.end(), audioFrame.begin());
    for (int i = 0; i < HOP_SIZE; ++i) {
        audioFrame[i + HOP_SIZE] = samples[i] / 32768.0f;
    }
    computeLogMel();
    if (frameCount < NUM_FRAMES) {
        ++frameCount;
    }
    ++framesSinceInference;
}

bool FeatureExtractor::begin(float scale, int32_t zeroPoint) {
    if (scale <= 0 || arm_rfft_fast_init_f32(&fft, FRAME_SIZE) != ARM_MATH_SUCCESS) {
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

const int8_t *FeatureExtractor::data() const {
    return featureBuffer.data();
}
