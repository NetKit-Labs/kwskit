#include "keyword_model.h"
#include <cstdint>
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_profiler.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include "kws_1class_depthwise_int8.h"
#include <algorithm>
#include <array>
#include <new>
#include <type_traits>

struct KeywordModel::Runtime {
    Runtime() = default;
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    alignas(16) std::array<std::uint8_t, 96 * 1024> tensorArena{};
    tflite::MicroErrorReporter errorReporter;
    tflite::MicroProfiler profiler;
    tflite::MicroMutableOpResolver<4> resolver;
    std::aligned_storage<sizeof(tflite::MicroInterpreter), alignof(tflite::MicroInterpreter)>::type
        interpreterStorage;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    bool ready = false;

    ~Runtime() {
        if (interpreter) {
            interpreter->~MicroInterpreter();
        }
    }
};

KeywordModel &KeywordModel::instance() {
    static Runtime storage;
    static KeywordModel model(storage);
    return model;
}

bool KeywordModel::begin() {
    if (runtime.interpreter) {
        return runtime.ready;
    }
    const auto *model = tflite::GetModel(kws_1class_depthwise_int8_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        return false;
    }
    if (runtime.resolver.AddConv2D() != kTfLiteOk ||
        runtime.resolver.AddDepthwiseConv2D() != kTfLiteOk ||
        runtime.resolver.AddSoftmax() != kTfLiteOk || runtime.resolver.AddMean() != kTfLiteOk) {
        return false;
    }

    // Placement construction uses the fixed member storage; it does not allocate from the heap.
    runtime.interpreter =
        new (&runtime.interpreterStorage) tflite::MicroInterpreter(model, runtime.resolver,
            runtime.tensorArena.data(), runtime.tensorArena.size(), nullptr, &runtime.profiler);
    if (runtime.interpreter->AllocateTensors() != kTfLiteOk) {
        static_cast<tflite::ErrorReporter &>(runtime.errorReporter)
            .Report("AllocateTensors() failed");
        return false;
    }
    runtime.input = runtime.interpreter->input(0);
    runtime.output = runtime.interpreter->output(0);
    runtime.ready = runtime.input->type == kTfLiteInt8 && runtime.output->type == kTfLiteInt8 &&
        runtime.input->bytes == NUM_FRAMES * NUM_MEL && runtime.output->bytes >= 1 &&
        runtime.input->params.scale > 0 && runtime.output->params.scale > 0;
    return runtime.ready;
}

float KeywordModel::inputScale() const {
    return runtime.input ? runtime.input->params.scale : 0.0f;
}

std::int32_t KeywordModel::inputZeroPoint() const {
    return runtime.input ? runtime.input->params.zero_point : 0;
}

bool KeywordModel::predict(const FeatureTensor &features, float &probability) {
    if (!runtime.ready) {
        return false;
    }
    std::copy(features.begin(), features.end(), runtime.input->data.int8);
    if (runtime.interpreter->Invoke() != kTfLiteOk) {
        return false;
    }
    probability = runtime.output->params.scale *
        (runtime.output->data.int8[0] - runtime.output->params.zero_point);
    return true;
}
