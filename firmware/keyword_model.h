#pragma once
#include "kws_config.h"
#include <cstdint>

// One fixed TFLite runtime for the deployed model, constructed on first use.
// Implementation storage stays private so consumers do not depend on TFLite headers.
class KeywordModel {
public:
    static KeywordModel &instance();
    KeywordModel(const KeywordModel &) = delete;
    KeywordModel &operator=(const KeywordModel &) = delete;

    bool begin();
    float inputScale() const;
    std::int32_t inputZeroPoint() const;
    bool predict(const FeatureTensor &features, float &probability);

private:
    struct Runtime;
    explicit KeywordModel(Runtime &runtime) : runtime(runtime) {}
    Runtime &runtime;
};
