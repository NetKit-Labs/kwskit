#pragma once
#include "kws_config.h"
namespace Features {
    bool begin(float scale, int32_t zeroPoint);
    void reset();
    void consume(const int16_t *audioBuffer);
    bool inferenceDue();
    void markInferred();
    const int8_t *data();
} // namespace Features
