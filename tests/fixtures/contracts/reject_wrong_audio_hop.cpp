// Compiler contract probe: wrong audio hop.
#include "common.h"

int main() {
    FeatureExtractor extractor;
    std::array<std::int16_t, HOP_SIZE - 1> hop{};
    extractor.consume(hop);
}
