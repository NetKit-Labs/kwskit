// ===============================
// Macros to enable DSP / CMSIS-NN
// ===============================
#define ARM_MATH_DSP           // Enable DSP-optimized math (SIMD/FPU on Cortex-M4)
#define ARM_MATH_LOOPUNROLL    // Enable loop unrolling in CMSIS-DSP
#define TF_LITE_USE_CMSIS_NN   // Force TensorFlow Lite Micro to use CMSIS-NN kernels for int8

// ===============================
// Include necessary libraries
// ===============================
#include <USB/PluggableUSBSerial.h>
#include <Arduino.h>           // Core Arduino functions
#include "audio_queue.h"
#include "fft_power.h"
#include "keyword_filter.h"
#include "timing_stats.h"
KeywordFilter keywordFilter;
bool deviceOn = false;
void applyKeywordAction() {
  deviceOn = !deviceOn;
  digitalWrite(LED_BUILTIN, deviceOn ? HIGH : LOW);
}
#include <PDM.h>               // PDM audio
#include <arm_math.h>          // CMSIS-DSP math functions (FFT, filters, etc.)
#include <TensorFlowLite.h>    // Core TensorFlow Lite Micro
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/micro/micro_profiler.h>

// Model include
#include "kws_1class_depthwise_int8.h"
#include "mel_filter_coefs.h"

// =======================
// Parameters
// =======================
#define SAMPLE_RATE 16000
#define FRAME_SIZE 512
#define HOP_SIZE (FRAME_SIZE / 2)
#define PDM_BUFFER_SIZE (FRAME_SIZE / 2)
#define SPECTRUM_BINS (FRAME_SIZE / 2 + 1)
#define NUM_MEL 20
#define NUM_FRAMES 61


// =======================
// Audio buffers
// =======================
int16_t pdmBuffer[PDM_BUFFER_SIZE];
int16_t audioBuffer[PDM_BUFFER_SIZE];

AudioQueue<8192> audioQueue; // 512 ms of audio at 16 kHz
int frameCounter = 0;
int framesSinceInference = 0;
constexpr int INFERENCE_HOP = 8; // 128 ms between decisions
bool firstFrame = true;
volatile uint32_t receivedSamples = 0, missedBuffers = 0, lastAudioUs = 0;
volatile uint32_t queueHighWater = 0, callbackGapMaxUs = 0;
TimingStats dspTiming, inferenceTiming, processingTiming, responseTiming;
uint32_t confirmedKeywords = 0, inferenceErrors = 0, seenDrops = 0;
uint32_t evidenceUs = 0, captureEndUs = 0;
bool evidenceActive = false;
float latestProbability = 0;

// Nano USB CDC nonblocking output: retain unsent bytes and never wait for a host.
char telemetry[512];
size_t telemetryLength = 0, telemetryOffset = 0;
void pumpTelemetry() {
  if (telemetryOffset >= telemetryLength || !_SerialUSB.connected()) return;
  uint32_t sent = 0;
  size_t remaining = telemetryLength - telemetryOffset;
  _SerialUSB.send_nb((uint8_t*)telemetry + telemetryOffset,
                 min(remaining, size_t(64)), &sent);
  telemetryOffset += sent;
}

float audioFrame[FRAME_SIZE];
float windowed[FRAME_SIZE];
float hannWindow[FRAME_SIZE];
float fftOut[FRAME_SIZE];
float mag[SPECTRUM_BINS];
int8_t quantized_mel_row[NUM_MEL];
int8_t featureBuffer[NUM_FRAMES * NUM_MEL];

// CMSIS-DSP matrix/vector instances
arm_matrix_instance_f32 melMat, magVec, melOut;
float melRowTmp[NUM_MEL];

// FFT instance
arm_rfft_fast_instance_f32 fft_inst;

// =======================
// TFLite variables
// =======================
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

// =======================
// PDM callback
// =======================
void onPDMData() {
  uint32_t now = micros();
  if (lastAudioUs) {
    uint32_t gap = now - lastAudioUs;
    if (gap > callbackGapMaxUs) callbackGapMaxUs = gap;
  }
  lastAudioUs = now;
  bool missed = false;
  while (PDM.available() > 0) {
    int bytes = min(PDM.available(), (int)sizeof(pdmBuffer));
    int received = PDM.read(pdmBuffer, bytes);
    if (received <= 0) break;
    receivedSamples += received / 2;
    for (int i = 0; i < received / 2; ++i)
      if (!audioQueue.push(pdmBuffer[i])) missed = true;
  }
  if (missed) ++missedBuffers;
  uint32_t depth = audioQueue.available();
  if (depth > queueHighWater) queueHighWater = depth;
}

// =======================
// Compute Hann window
// =======================
void computeHannWindow() {
  for (int n = 0; n < FRAME_SIZE; n++)
    hannWindow[n] = 0.5f - 0.5f * cosf(2.0f * PI * n / (FRAME_SIZE - 1));
}

// =======================
// Compute log-Mel for one frame
// =======================
void computeLogMel() {
  arm_mult_f32(audioFrame, hannWindow, windowed, FRAME_SIZE);
  arm_rfft_fast_f32(&fft_inst, windowed, fftOut, 0);

  fftPower(fftOut, mag, FRAME_SIZE);

  magVec.pData = mag;
  melOut.pData = melRowTmp;

  arm_mat_mult_f32(&melMat, &magVec, &melOut);

  for (int m = 0; m < NUM_MEL; m++) {
    float log_mel = logf(melRowTmp[m] + 1e-6f);
    int32_t q = (int32_t)roundf(log_mel / input_scale + input_zero_point);
    quantized_mel_row[m] = (int8_t)constrain(q, -128, 127);
  }

  memmove(featureBuffer, featureBuffer + NUM_MEL, (NUM_FRAMES - 1) * NUM_MEL * sizeof(int8_t));
  memcpy(featureBuffer + (NUM_FRAMES - 1) * NUM_MEL, quantized_mel_row, NUM_MEL * sizeof(int8_t));
}

// =======================
// Process audio buffer
// =======================
void processAudio() {
  if (firstFrame) {
    memset(audioFrame, 0, sizeof(audioFrame));
    firstFrame = false;
    for (int i = 0; i < HOP_SIZE; i++)
      audioFrame[i + HOP_SIZE] = audioBuffer[i] / 32768.0f;
  } else {
    memmove(audioFrame, audioFrame + HOP_SIZE, HOP_SIZE * sizeof(float));
    for (int i = 0; i < HOP_SIZE; i++)
      audioFrame[i + HOP_SIZE] = audioBuffer[i] / 32768.0f;

    computeLogMel();
    if (frameCounter < NUM_FRAMES) ++frameCounter;
    ++framesSinceInference;
  }
}

// =======================
// Arduino setup
// =======================
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(115200);
  uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 2000) {}

  arm_rfft_fast_init_f32(&fft_inst, FRAME_SIZE);
  arm_mat_init_f32(&melMat, NUM_MEL, SPECTRUM_BINS, (float*)melFilterbank);
  arm_mat_init_f32(&magVec, SPECTRUM_BINS, 1, mag);
  arm_mat_init_f32(&melOut, NUM_MEL, 1, melRowTmp);

  computeHannWindow();


  // Load TFLite model
  model = tflite::GetModel(kws_1class_depthwise_int8_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) while (1);

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
    while (1);
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  input_scale = input->params.scale;
  input_zero_point = input->params.zero_point;
  output_scale = output->params.scale;
  output_zero_point = output->params.zero_point;

  PDM.onReceive(onPDMData);
  PDM.setBufferSize(sizeof(pdmBuffer));
  if (!PDM.begin(1, SAMPLE_RATE)) { Serial.println("PDM failed"); while (1) {} }
  PDM.setGain(60);
  Serial.println("Setup complete.");
}

// =======================
// Arduino loop
// =======================
void loop() {
  uint32_t processingStart = micros();
  pumpTelemetry();
  noInterrupts();
  uint32_t dropped = audioQueue.droppedSamples;
  interrupts();
  if (dropped != seenDrops) {
    // Do not classify a window containing an audio discontinuity.
    seenDrops = dropped;
    int16_t discarded[HOP_SIZE];
    while (audioQueue.pop(discarded, HOP_SIZE)) {}
    while (audioQueue.pop(discarded, 1)) {}
    firstFrame = true;
    frameCounter = framesSinceInference = 0;
    keywordFilter = KeywordFilter();
    evidenceActive = false;
  }
  if (audioQueue.pop(audioBuffer, HOP_SIZE)) {
    noInterrupts();
    uint32_t arrival = lastAudioUs;
    uint32_t backlog = audioQueue.available();
    interrupts();
    captureEndUs = arrival - (backlog * 1000000UL) / SAMPLE_RATE;
    uint32_t dspStart = micros();
    processAudio();
    dspTiming.record(micros() - dspStart);

    if (frameCounter >= NUM_FRAMES && framesSinceInference >= INFERENCE_HOP
        && audioQueue.available() < HOP_SIZE) {
      framesSinceInference = 0;
      memcpy(input->data.int8, featureBuffer, sizeof(featureBuffer));
      uint32_t inferenceStart = micros();
      TfLiteStatus status = interpreter->Invoke();
      inferenceTiming.record(micros() - inferenceStart);
      if (status != kTfLiteOk) {
        ++inferenceErrors;
        keywordFilter = KeywordFilter();
        evidenceActive = false;
      } else {
        latestProbability = output_scale * (output->data.int8[0] - output_zero_point);
        if (latestProbability >= 0.8f && !evidenceActive) {
          evidenceUs = captureEndUs;
          evidenceActive = true;
        } else if (latestProbability < 0.4f) evidenceActive = false;
        if (keywordFilter.update(latestProbability, millis())) {
          applyKeywordAction();
          ++confirmedKeywords;
          if (evidenceActive) responseTiming.record(micros() - evidenceUs);
        }
      }
    }
    processingTiming.record(micros() - processingStart);
  }

  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 2000 && telemetryOffset >= telemetryLength) {
    lastReport = millis();
    noInterrupts();
    uint32_t samples = receivedSamples, missed = missedBuffers;
    uint32_t high = queueHighWater, gap = callbackGapMaxUs, arrival = lastAudioUs;
    dropped = audioQueue.droppedSamples;
    interrupts();
    int length = snprintf(telemetry, sizeof(telemetry),
      "ms=%lu samples=%lu dropped=%lu missed_buffers=%lu queue_max=%lu gap_max_us=%lu audio_age_us=%lu "
      "dsp_us=%lu/%lu infer_us=%lu/%lu process_us=%lu/%lu response_us=%lu/%lu "
      "inferences=%lu errors=%lu p_milli=%d triggers=%lu led=%d\n",
      (unsigned long)millis(), (unsigned long)samples, (unsigned long)dropped,
      (unsigned long)missed, (unsigned long)high, (unsigned long)gap,
      (unsigned long)(micros()-arrival),
      (unsigned long)dspTiming.average(), (unsigned long)dspTiming.maximum,
      (unsigned long)inferenceTiming.average(), (unsigned long)inferenceTiming.maximum,
      (unsigned long)processingTiming.average(), (unsigned long)processingTiming.maximum,
      (unsigned long)responseTiming.last, (unsigned long)responseTiming.maximum,
      (unsigned long)inferenceTiming.count, (unsigned long)inferenceErrors,
      int(latestProbability*1000), (unsigned long)confirmedKeywords, int(deviceOn));
    telemetryLength = length > 0 ? min(size_t(length), sizeof(telemetry)-1) : 0;
    telemetryOffset = 0;
  }
}
