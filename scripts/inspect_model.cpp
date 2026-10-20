// Inspect the project's single-subgraph TFLite artifact using the pinned schema.
// This is a host tool; it does not link or execute the inference runtime.
#include "../firmware/cpp_standard.h"
#include <tensorflow/lite/schema/schema_generated.h>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <vector>
#include <string_view>

namespace {
    void string(std::string_view value) {
        std::cout << '"';
        for (char character : value) {
            const auto byte = static_cast<unsigned char>(character);
            if (byte == '"' || byte == '\\')
                std::cout << '\\' << character;
            else if (byte < 32)
                std::cout << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(byte) << std::dec;
            else
                std::cout << character;
        }
        std::cout << '"';
    }
    template <typename T> void array(const flatbuffers::Vector<T> *values) {
        std::cout << '[';
        if (values)
            for (std::size_t i = 0; i < values->size(); ++i) {
                if (i)
                    std::cout << ',';
                std::cout << values->Get(i);
            }
        std::cout << ']';
    }
    std::uint64_t elements(const tflite::Tensor &tensor) {
        std::uint64_t count = 1;
        for (int dim : *tensor.shape())
            count *= dim;
        return count;
    }
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: inspect_model <model.tflite>\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open model");
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        if (!tflite::VerifyModelBuffer(verifier))
            throw std::runtime_error("Invalid TFLite FlatBuffer");
        const auto &model = *tflite::GetModel(bytes.data());
        if (!model.subgraphs() || model.subgraphs()->size() != 1)
            throw std::runtime_error("This inspector requires one subgraph");
        const auto &graph = *model.subgraphs()->Get(0);
        std::set<int> weights, biases;
        std::cout << std::setprecision(10) << "{\"model_bytes\":" << bytes.size()
                  << ",\"schema_version\":" << model.version() << ",\"inputs\":";
        array(graph.inputs());
        std::cout << ",\"outputs\":";
        array(graph.outputs());
        std::cout << ",\"operators\":[";
        for (std::size_t i = 0; i < graph.operators()->size(); ++i) {
            const auto &op = *graph.operators()->Get(i);
            const auto &code = *model.operator_codes()->Get(op.opcode_index());
            const auto builtin =
                static_cast<tflite::BuiltinOperator>(std::max(static_cast<int>(code.builtin_code()),
                    static_cast<int>(code.deprecated_builtin_code())));
            if (i)
                std::cout << ',';
            std::cout << "{\"index\":" << i << ",\"operator\":";
            string(tflite::EnumNameBuiltinOperator(builtin));
            std::cout << ",\"version\":" << code.version() << ",\"inputs\":";
            array(op.inputs());
            std::cout << ",\"outputs\":";
            array(op.outputs());
            if (builtin == tflite::BuiltinOperator_CONV_2D ||
                builtin == tflite::BuiltinOperator_DEPTHWISE_CONV_2D) {
                weights.insert(op.inputs()->Get(1));
                if (op.inputs()->size() > 2 && op.inputs()->Get(2) >= 0)
                    biases.insert(op.inputs()->Get(2));
                int h, w, activation, padding;
                if (builtin == tflite::BuiltinOperator_CONV_2D) {
                    const auto &options = *op.builtin_options_as_Conv2DOptions();
                    h = options.stride_h();
                    w = options.stride_w();
                    activation = options.fused_activation_function();
                    padding = options.padding();
                } else {
                    const auto &options = *op.builtin_options_as_DepthwiseConv2DOptions();
                    h = options.stride_h();
                    w = options.stride_w();
                    activation = options.fused_activation_function();
                    padding = options.padding();
                    std::cout << ",\"depth_multiplier\":" << options.depth_multiplier();
                }
                std::cout << ",\"stride_hw\":[" << h << ',' << w << "],\"padding\":";
                string(tflite::EnumNamePadding(static_cast<tflite::Padding>(padding)));
                std::cout << ",\"activation\":";
                string(tflite::EnumNameActivationFunctionType(
                    static_cast<tflite::ActivationFunctionType>(activation)));
            }
            if (builtin == tflite::BuiltinOperator_MEAN) {
                std::cout << ",\"keep_dims\":"
                          << (op.builtin_options_as_ReducerOptions()->keep_dims() ? "true"
                                                                                  : "false");
                const auto &axesTensor = *graph.tensors()->Get(op.inputs()->Get(1));
                const auto &axes = *model.buffers()->Get(axesTensor.buffer())->data();
                std::cout << ",\"axes\":[";
                for (std::size_t axis = 0; axis < axes.size() / sizeof(std::int32_t); ++axis) {
                    if (axis)
                        std::cout << ',';
                    std::cout << flatbuffers::ReadScalar<std::int32_t>(
                        axes.data() + axis * sizeof(std::int32_t));
                }
                std::cout << ']';
            }
            std::cout << '}';
        }
        std::uint64_t weightCount = 0, biasCount = 0, parameterBytes = 0;
        std::cout << "],\"tensors\":[";
        for (std::size_t i = 0; i < graph.tensors()->size(); ++i) {
            const auto &tensor = *graph.tensors()->Get(i);
            const auto &buffer = *model.buffers()->Get(tensor.buffer());
            const auto size = buffer.data() ? buffer.data()->size() : 0;
            if (weights.count(i)) {
                weightCount += elements(tensor);
                parameterBytes += size;
            }
            if (biases.count(i)) {
                biasCount += elements(tensor);
                parameterBytes += size;
            }
            if (i)
                std::cout << ',';
            std::cout << "{\"index\":" << i << ",\"name\":";
            string(tensor.name() ? tensor.name()->c_str() : "");
            std::cout << ",\"type\":";
            string(tflite::EnumNameTensorType(tensor.type()));
            std::cout << ",\"shape\":";
            array(tensor.shape());
            std::cout << ",\"constant_bytes\":" << size
                      << ",\"variable\":" << (tensor.is_variable() ? "true" : "false");
            if (const auto *parameters = tensor.quantization()) {
                const auto &quantization = *parameters;
                std::cout << ",\"scales\":";
                array(quantization.scale());
                std::cout << ",\"zero_points\":";
                array(quantization.zero_point());
                std::cout << ",\"quantized_dimension\":" << quantization.quantized_dimension();
            }
            std::cout << '}';
        }
        std::cout << "],\"convolution_weight_elements\":" << weightCount
                  << ",\"convolution_bias_elements\":" << biasCount
                  << ",\"convolution_parameter_bytes\":" << parameterBytes << "}\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
