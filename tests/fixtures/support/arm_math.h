#pragma once
#include <cstdint>

enum arm_status { ARM_MATH_SUCCESS = 0, ARM_MATH_ARGUMENT_ERROR = -1 };
struct arm_matrix_instance_f32 {
    std::uint16_t numRows, numCols;
    float *pData;
};
struct arm_rfft_fast_instance_f32 {
    std::uint16_t fftLen;
};
arm_status arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32 *, std::uint16_t);
void arm_rfft_fast_f32(arm_rfft_fast_instance_f32 *, float *, float *, std::uint8_t);
void arm_mult_f32(const float *, const float *, float *, std::uint32_t);
void arm_mat_init_f32(arm_matrix_instance_f32 *, std::uint16_t, std::uint16_t, float *);
arm_status arm_mat_mult_f32(
    const arm_matrix_instance_f32 *, const arm_matrix_instance_f32 *, arm_matrix_instance_f32 *);
