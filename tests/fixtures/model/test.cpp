#include "keyword_model.h"
#include <TensorFlowLite.h>
#include <cassert>
#include <limits>
#include <initializer_list>
#include <string_view>

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string_view scenario{argv[1]};
    FakeModel::input.bytes = NUM_FRAMES * NUM_MEL;
    FakeModel::input.params.scale = 0.09529872239f;
    FakeModel::input.params.zero_point = 17;
    FakeModel::input.data.int8 = FakeModel::inputData.data();
    FakeModel::output.bytes = 3;
    FakeModel::output.params.scale = 1.0f / 256;
    FakeModel::output.params.zero_point = -128;
    FakeModel::output.data.int8 = FakeModel::outputData.data();
    if (scenario == "schema")
        FakeModel::version = 0;
    else if (scenario == "registration")
        FakeModel::registration = false;
    else if (scenario == "allocation")
        FakeModel::allocation = false;
    else if (scenario == "null-input")
        FakeModel::haveInput = false;
    else if (scenario == "null-output")
        FakeModel::haveOutput = false;
    else if (scenario == "input-type")
        FakeModel::input.type = 0;
    else if (scenario == "output-type")
        FakeModel::output.type = 0;
    else if (scenario == "input-size")
        --FakeModel::input.bytes;
    else if (scenario == "output-size")
        FakeModel::output.bytes = 0;
    else if (scenario == "input-data")
        FakeModel::input.data.int8 = nullptr;
    else if (scenario == "output-data")
        FakeModel::output.data.int8 = nullptr;
    else if (scenario == "input-scale")
        FakeModel::input.params.scale = std::numeric_limits<float>::quiet_NaN();
    else if (scenario == "output-scale")
        FakeModel::output.params.scale = std::numeric_limits<float>::infinity();
    else if (scenario == "input-zero")
        FakeModel::input.params.zero_point = -129;
    else if (scenario == "output-zero")
        FakeModel::output.params.zero_point = std::numeric_limits<std::int32_t>::min();
    else
        assert(scenario == "normal");
    auto &model = KeywordModel::instance();
    FeatureTensor features{};
    float probability = 0.5f;
    assert(!model.predict(features, probability) && probability == 0.5f);
    const bool valid = scenario == "normal";
    assert(model.begin() == valid);
    assert(model.begin() == valid); // Construction/failed initialization is not repeated.
    if (!valid) {
        assert(!model.predict(features, probability) && probability == 0.5f);
        assert(FakeModel::invokes == 0);
        return 0;
    }
    assert(model.inputScale() == FakeModel::input.params.scale && model.inputZeroPoint() == 17);
    for (std::size_t i = 0; i < features.size(); ++i)
        features[i] = static_cast<std::int8_t>(static_cast<int>(i % 256) - 128);
    FakeModel::outputData[0] = 127;
    assert(model.predict(features, probability) && probability == 255.0f / 256);
    assert(FakeModel::inputData == features);
    const auto previous = probability;
    FakeModel::invocation = false;
    assert(!model.predict(features, probability) && probability == previous);
    FakeModel::invocation = true;
    for (const auto scale :
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::quiet_NaN(), -1.0f, 1.0f}) {
        FakeModel::output.params.scale = scale;
        assert(!model.predict(features, probability) && probability == previous);
    }
}
