#include "keyword_model.h"
#include <Arduino.h>
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_profiler.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include "kws_1class_depthwise_int8.h"
namespace KeywordModel {
namespace {
constexpr int kTensorArenaSize = 96 * 1024;
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter* error_reporter = &micro_error_reporter;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
static tflite::MicroProfiler profiler;

float input_scale;
int32_t input_zero_point;
float output_scale;
int32_t output_zero_point;

}
bool begin() {
  // Load TFLite model
  model = tflite::GetModel(kws_1class_depthwise_int8_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) return false;

  // =======================
  // Ops resolver (int8 CMSIS-NN will be used automatically)
  // =======================
  static tflite::MicroMutableOpResolver<4> resolver;
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddSoftmax();
  resolver.AddMean(); // optional; reference kernel

  // Interpreter
  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, kTensorArenaSize, nullptr, &profiler);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    error_reporter->Report("AllocateTensors() failed");
    return false;
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  input_scale = input->params.scale;
  input_zero_point = input->params.zero_point;
  output_scale = output->params.scale;
  output_zero_point = output->params.zero_point;

  if (input->type != kTfLiteInt8 || output->type != kTfLiteInt8 ||
      input->bytes != NUM_FRAMES * NUM_MEL || output->bytes < 1 ||
      input_scale <= 0 || output_scale <= 0) return false;
  return true;
}
float inputScale() { return input_scale; }
int32_t inputZeroPoint() { return input_zero_point; }
bool predict(const int8_t* features, float& probability) {
  memcpy(input->data.int8, features, NUM_FRAMES * NUM_MEL);
  if (interpreter->Invoke() != kTfLiteOk) return false;
  probability = output_scale * (output->data.int8[0] - output_zero_point);
  return true;
}
}
