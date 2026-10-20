#include "feature_extractor.h"
#include "fft_power.h"
#include <Arduino.h>
#include <arm_math.h>
#include "mel_filter_coefs.h"
namespace Features {
    namespace {
        float input_scale;
        int32_t input_zero_point;
        int frameCounter = 0, framesSinceInference = 0;
        bool firstFrame = true;
        float audioFrame[FRAME_SIZE];
        float windowed[FRAME_SIZE];
        float hannWindow[FRAME_SIZE];
        float fftOut[FRAME_SIZE];
        float mag[SPECTRUM_BINS];
        int8_t quantized_mel_row[NUM_MEL];
        int8_t featureBuffer[NUM_FRAMES * NUM_MEL];

        // CMSIS-DSP matrix/vector instances
        arm_matrix_instance_f32 melMat, magVec, melOut;
        float melRowTmp[NUM_MEL];

        // FFT instance
        arm_rfft_fast_instance_f32 fft_inst;

        void computeHannWindow() {
            for (int n = 0; n < FRAME_SIZE; n++)
                hannWindow[n] = 0.5f - 0.5f * cosf(2.0f * PI * n / (FRAME_SIZE - 1));
        }

        // =======================
        // Compute log-Mel for one frame
        // =======================
        void computeLogMel() {
            arm_mult_f32(audioFrame, hannWindow, windowed, FRAME_SIZE);
            arm_rfft_fast_f32(&fft_inst, windowed, fftOut, 0);

            fftPower(fftOut, mag, FRAME_SIZE);

            magVec.pData = mag;
            melOut.pData = melRowTmp;

            arm_mat_mult_f32(&melMat, &magVec, &melOut);

            for (int m = 0; m < NUM_MEL; m++) {
                float log_mel = logf(melRowTmp[m] + 1e-6f);
                int32_t q = (int32_t)roundf(log_mel / input_scale + input_zero_point);
                quantized_mel_row[m] = (int8_t)constrain(q, -128, 127);
            }

            memmove(featureBuffer, featureBuffer + NUM_MEL,
                (NUM_FRAMES - 1) * NUM_MEL * sizeof(int8_t));
            memcpy(featureBuffer + (NUM_FRAMES - 1) * NUM_MEL, quantized_mel_row,
                NUM_MEL * sizeof(int8_t));
        }

        // =======================
        // Process audio buffer
        // =======================
        void consumeInternal(const int16_t *audioBuffer) {
            if (firstFrame) {
                memset(audioFrame, 0, sizeof(audioFrame));
                firstFrame = false;
                for (int i = 0; i < HOP_SIZE; i++)
                    audioFrame[i + HOP_SIZE] = audioBuffer[i] / 32768.0f;
            } else {
                memmove(audioFrame, audioFrame + HOP_SIZE, HOP_SIZE * sizeof(float));
                for (int i = 0; i < HOP_SIZE; i++)
                    audioFrame[i + HOP_SIZE] = audioBuffer[i] / 32768.0f;

                computeLogMel();
                if (frameCounter < NUM_FRAMES)
                    ++frameCounter;
                ++framesSinceInference;
            }
        }

    } // namespace
    bool begin(float scale, int32_t zeroPoint) {
        input_scale = scale;
        input_zero_point = zeroPoint;
        if (scale <= 0 || arm_rfft_fast_init_f32(&fft_inst, FRAME_SIZE) != ARM_MATH_SUCCESS)
            return false;
        arm_mat_init_f32(&melMat, NUM_MEL, SPECTRUM_BINS, (float *)melFilterbank);
        arm_mat_init_f32(&magVec, SPECTRUM_BINS, 1, mag);
        arm_mat_init_f32(&melOut, NUM_MEL, 1, melRowTmp);
        computeHannWindow();
        reset();
        return true;
    }
    void reset() {
        firstFrame = true;
        frameCounter = framesSinceInference = 0;
    }
    void consume(const int16_t *samples) {
        consumeInternal(samples);
    }
    bool inferenceDue() {
        return frameCounter >= NUM_FRAMES && framesSinceInference >= INFERENCE_HOP;
    }
    void markInferred() {
        framesSinceInference = 0;
    }
    const int8_t *data() {
        return featureBuffer;
    }
} // namespace Features
