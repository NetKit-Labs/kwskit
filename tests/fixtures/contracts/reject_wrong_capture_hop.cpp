// Compiler contract probe: wrong capture hop.
#include "common.h"

int main() {
    AudioCapture capture;
    std::array<std::int16_t, HOP_SIZE + 1> hop{};
    std::uint32_t t = 0;
    capture.readHop(hop, t);
}
