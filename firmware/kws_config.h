#pragma once
#define ARM_MATH_DSP 1
#define ARM_MATH_LOOPUNROLL
#define TF_LITE_USE_CMSIS_NN
#include <stdint.h>
constexpr int SAMPLE_RATE = 16000;
constexpr int FRAME_SIZE = 512;
constexpr int HOP_SIZE = FRAME_SIZE / 2;
constexpr int PDM_BUFFER_SIZE = HOP_SIZE;
constexpr int SPECTRUM_BINS = FRAME_SIZE / 2 + 1;
constexpr int NUM_MEL = 20;
constexpr int NUM_FRAMES = 61;
constexpr int INFERENCE_HOP = 8;
