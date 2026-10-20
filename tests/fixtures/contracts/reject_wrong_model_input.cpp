// Compiler contract probe: wrong model input.
#include "common.h"

int main() {
    auto &model = KeywordModel::instance();
    std::array<std::int8_t, NUM_FRAMES * NUM_MEL - 1> features{};
    float p = 0;
    model.predict(features, p);
}
