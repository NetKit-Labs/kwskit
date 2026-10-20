#pragma once
#include <array>
#include <cstddef>

// CMSIS fast real FFT packs DC at [0] and the real Nyquist bin at [1].
// Array extents enforce the input/output relationship at compile time.
template <std::size_t Size>
void fftPower(const std::array<float, Size> &packed, std::array<float, Size / 2 + 1> &power) {
    static_assert(Size >= 2 && Size % 2 == 0, "An even real-FFT length is required");
    power[0] = packed[0] * packed[0];
    for (std::size_t k = 1; k < Size / 2; ++k) {
        power[k] = packed[2 * k] * packed[2 * k] + packed[2 * k + 1] * packed[2 * k + 1];
    }
    power[Size / 2] = packed[1] * packed[1];
}
