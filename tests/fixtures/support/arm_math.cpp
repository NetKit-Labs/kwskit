#include "arm_math.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <utility>

// Scalar host backend for the integration test, not the board's CMSIS implementation.
bool failFftInitialization = false;
bool failMelMultiplication = false;
float injectedMel = 0.0f;
std::array<float, 512> lastAppliedWindow{};

arm_status arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32 *fft, std::uint16_t length) {
    if (failFftInitialization || length != 512) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    fft->fftLen = length;
    return ARM_MATH_SUCCESS;
}

void arm_mult_f32(const float *left, const float *right, float *output, std::uint32_t length) {
    for (std::uint32_t i = 0; i < length; ++i) {
        lastAppliedWindow[i] = right[i];
        output[i] = left[i] * right[i];
    }
}

void arm_rfft_fast_f32(arm_rfft_fast_instance_f32 *, float *input, float *output, std::uint8_t) {
    std::array<std::complex<float>, 512> values;
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = input[i];
    }
    for (std::size_t i = 1, j = 0; i < values.size(); ++i) {
        std::size_t bit = values.size() >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(values[i], values[j]);
        }
    }
    for (std::size_t length = 2; length <= values.size(); length *= 2) {
        const float angle = -6.2831853071795864769f / length;
        const std::complex<float> rotation(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < values.size(); i += length) {
            std::complex<float> phase(1.0f, 0.0f);
            for (std::size_t j = 0; j < length / 2; ++j) {
                const auto even = values[i + j];
                const auto odd = values[i + j + length / 2] * phase;
                values[i + j] = even + odd;
                values[i + j + length / 2] = even - odd;
                phase *= rotation;
            }
        }
    }
    output[0] = values[0].real();
    output[1] = values[256].real();
    for (std::size_t i = 1; i < 256; ++i) {
        output[2 * i] = values[i].real();
        output[2 * i + 1] = values[i].imag();
    }
}

void arm_mat_init_f32(
    arm_matrix_instance_f32 *matrix, std::uint16_t rows, std::uint16_t columns, float *data) {
    *matrix = {rows, columns, data};
}

arm_status arm_mat_mult_f32(const arm_matrix_instance_f32 *left,
    const arm_matrix_instance_f32 *right, arm_matrix_instance_f32 *output) {
    if (failMelMultiplication)
        return ARM_MATH_ARGUMENT_ERROR;
    for (std::uint16_t row = 0; row < left->numRows; ++row) {
        float sum = 0.0f;
        for (std::uint16_t column = 0; column < left->numCols; ++column) {
            sum += left->pData[row * left->numCols + column] * right->pData[column];
        }
        output->pData[row] = sum + injectedMel;
    }
    return ARM_MATH_SUCCESS;
}
