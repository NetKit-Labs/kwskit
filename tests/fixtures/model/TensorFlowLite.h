#pragma once

#include "kws_config.h"

#include <array>
#include <cstddef>
#include <cstdint>

#ifndef TF_LITE_STRIP_ERROR_STRINGS
#error "Inference error logging must not use blocking Serial output"
#endif
constexpr int kTfLiteOk = 0, kTfLiteError = 1, kTfLiteInt8 = 9, TFLITE_SCHEMA_VERSION = 3;

struct TfLiteTensor {
    int type = kTfLiteInt8;
    std::size_t bytes = 0;

    struct {
        float scale = 1.0f;
        std::int32_t zero_point = 0;
    } params;

    struct {
        std::int8_t *int8 = nullptr;
    } data;
};

namespace FakeModel {
    inline int version = TFLITE_SCHEMA_VERSION, invokes = 0;
    inline bool registration = true, allocation = true, invocation = true;
    inline bool haveInput = true, haveOutput = true;
    inline FeatureTensor inputData{};
    inline std::array<std::int8_t, 3> outputData{};
    inline TfLiteTensor input, output;
} // namespace FakeModel

namespace tflite {
    class ErrorReporter {
    public:
        int Report(const char *, ...) {
            return 0;
        }
    };

    class MicroErrorReporter : public ErrorReporter {};

    class Model {
    public:
        int version() const {
            return FakeModel::version;
        }
    };

    inline const Model *GetModel(const unsigned char *) {
        static Model model;
        return &model;
    }

    template <int Count> class MicroMutableOpResolver {
    public:
        int AddConv2D() {
            return FakeModel::registration ? kTfLiteOk : kTfLiteError;
        }

        int AddDepthwiseConv2D() {
            return AddConv2D();
        }

        int AddSoftmax() {
            return AddConv2D();
        }

        int AddMean() {
            return AddConv2D();
        }
    };

    class MicroInterpreter {
    public:
        MicroInterpreter(const Model *, MicroMutableOpResolver<4> &, std::uint8_t *, std::size_t) {}

        std::size_t arena_used_bytes() const {
            return 12500;
        }

        int AllocateTensors() {
            return FakeModel::allocation ? kTfLiteOk : kTfLiteError;
        }

        TfLiteTensor *input(int) {
            return FakeModel::haveInput ? &FakeModel::input : nullptr;
        }

        TfLiteTensor *output(int) {
            return FakeModel::haveOutput ? &FakeModel::output : nullptr;
        }

        int Invoke() {
            ++FakeModel::invokes;
            return FakeModel::invocation ? kTfLiteOk : kTfLiteError;
        }
    };
} // namespace tflite
