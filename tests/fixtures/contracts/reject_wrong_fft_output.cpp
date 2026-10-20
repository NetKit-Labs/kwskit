// Compiler contract probe: wrong fft output.
#include "common.h"

int main() {
    std::array<float, 512> input{};
    std::array<float, 258> output{};
    fftPower(input, output);
}
