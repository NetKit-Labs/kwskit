// Compiler contract probe: copy extractor owner.
#include "common.h"

int main() {
    FeatureExtractor extractor;
    auto copy = extractor;
    (void)copy;
}
