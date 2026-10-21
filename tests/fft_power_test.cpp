#include "../firmware/fft_power.h"

#include <cassert>

int main() {
    std::array<float, 512> packed{};
    std::array<float, 257> power{};
    packed[0] = 512;
    fftPower(packed, power);
    assert(power[0] == 512 * 512 && power[256] == 0);
    packed[0] = 0;
    packed[1] = 512;
    fftPower(packed, power);
    assert(power[0] == 0 && power[256] == 512 * 512);
    packed[1] = 0;
    packed[14] = 3;
    packed[15] = 4;
    fftPower(packed, power);
    assert(power[7] == 25);
    for (int i = 0; i < 257; ++i)
        if (i != 7)
            assert(power[i] == 0);
    // Both packed edges must coexist without confusing DC/Nyquist with complex bins.
    std::array<float, 512> edges{};
    edges[0] = 7;
    edges[1] = -11;
    edges[2] = 5;
    edges[3] = -12;
    edges[510] = -8;
    edges[511] = 15;
    fftPower(edges, power);
    assert(power[0] == 49 && power[256] == 121);
    assert(power[1] == 169 && power[255] == 289);
    for (int i = 2; i < 255; ++i)
        assert(power[i] == 0);
}
