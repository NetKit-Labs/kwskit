#pragma once
#include "kws_config.h"
namespace KeywordModel {
    bool begin();
    float inputScale();
    int32_t inputZeroPoint();
    bool predict(const int8_t *features, float &probability);
} // namespace KeywordModel
