#pragma once
#include <stddef.h>
// CMSIS fast real FFT packs DC at [0] and the real Nyquist bin at [1].
inline void fftPower(const float* packed, float* power, size_t size) {
  power[0] = packed[0] * packed[0];
  for (size_t k = 1; k < size / 2; ++k)
    power[k] = packed[2*k] * packed[2*k] + packed[2*k+1] * packed[2*k+1];
  power[size/2] = packed[1] * packed[1];
}
