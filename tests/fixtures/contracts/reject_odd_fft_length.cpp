// Compiler contract probe: odd fft length.
#include "common.h"

int main() {
    std::array<float, 511> input{};
    std::array<float, 256> output{};
    fftPower(input, output);
}
