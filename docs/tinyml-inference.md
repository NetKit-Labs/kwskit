# TinyML model and neural-network inference

The application uses a fully convolutional, depthwise-separable CNN to classify
approximately one second of log-mel audio features. It is trained for three
classes: `yes` (index 0), unknown speech (index 1), and silence (index 2). The
firmware uses the class-0 score for filtered voice control. Training/export are
host operations; the Nano runs the fixed, quantized network for inference.

## Model identity and artifacts

| Item | Current artifact |
|---|---|
| Training definition and saved run | [kws_1class_depthwise.ipynb](../training/kws_1class_depthwise.ipynb) |
| Serialized inference model | [kws_1class_depthwise_int8.tflite](../models/kws_1class_depthwise_int8.tflite) |
| Firmware's compiled model array | [kws_1class_depthwise_int8.h](../firmware/kws_1class_depthwise_int8.h) |
| Runtime integration | [keyword_model.cpp](../firmware/keyword_model.cpp) |
| Inspected graph/tensor metadata | [model-metadata.json](model-metadata.json) |
| Host inspection source | [inspect_model.cpp](../scripts/inspect_model.cpp) |
| FlatBuffer size / schema version | 8496 bytes / version 3 |
| Graph size | One subgraph, eight operators, 22 tensors |
| Input | INT8 `[1, 61, 20, 1]`: batch, time frames, mel bands, channels; 1220 bytes |
| Output | INT8 `[1, 3]`: three quantized softmax scores |

The `.tflite` file and the bytes in the firmware header were compared and are
identical. Their SHA-256 is:

```text
5fcd68259fe9a636a365ca494118922fd020eaa4b4d6acfa26878a0c37a356a8
```

The firmware compiles the header array into the image; it does not open the
repository's `.tflite` file at runtime. A model update must replace the binary
artifact and matching header, then rebuild/upload the firmware. Preserve this
identity check when changing either file.

## Architecture and parameter counts

The deployed graph was inspected directly using the pinned TFLite schema. The
first five operators fuse ReLU with their convolution. Depthwise convolutions
have depth multiplier 1. All spatial dimensions below include a batch of one.

| Stage | Operation | Output shape | Weight values | Bias values |
|---|---|---|---:|---:|
| Initial convolution | 3 x 3, stride 2, SAME, 1 to 16 channels, ReLU | `[1,31,10,16]` | 144 | 16 |
| Depthwise block 1 | 3 x 3, stride 1, SAME, ReLU | `[1,31,10,16]` | 144 | 16 |
| Pointwise block 1 | 1 x 1, stride 1, 16 to 16 channels, ReLU | `[1,31,10,16]` | 256 | 16 |
| Depthwise block 2 | 3 x 3, stride 1, SAME, ReLU | `[1,31,10,16]` | 144 | 16 |
| Pointwise block 2 | 1 x 1, stride 1, 16 to 16 channels, ReLU | `[1,31,10,16]` | 256 | 16 |
| Classifier | 1 x 1, stride 1, 16 to 3 channels, no activation | `[1,31,10,3]` | 48 | 3 |
| Global average pooling | MEAN over spatial axes 1 and 2 | `[1,3]` | 0 | 0 |
| Class scores | SOFTMAX | `[1,3]` | 0 | 0 |
| Deployed total | Six convolution operators | | **992** | **83** |

Pointwise convolutions use VALID padding in the first two blocks; for a 1 x 1
kernel with stride 1, their spatial dimensions remain unchanged. The exported
classifier uses SAME padding. There is no dense/fully connected operator.

There are two relevant parameter counts:

- **Training model: 1315 total Keras parameters**, comprising 1155 trainable and
  160 non-trainable values, as recorded by `model.summary()` in the notebook.
  This includes five batch-normalization layers, each with 16 channels and four
  values per channel: trainable scale/offset plus running mean/variance.
- **Deployed model: 1075 convolution parameter values**, comprising 992 int8
  weights and 83 int32 biases. Batch-normalization transforms are folded into
  the convolution parameters; the deployed graph has no separate batch-normalization
  operator. Training dropout (rate 0.3) also has no inference operator or parameters.

The deployed parameter payload occupies **1324 bytes**: `992 x 1 + 83 x 4`.
The full **8496-byte** FlatBuffer additionally contains graph/tensor descriptions,
quantization scales/zero points, operator settings, metadata, alignment, and a
two-int32 reduction-axis constant. Parameter count, parameter bytes, serialized
model bytes, and inference working RAM are different quantities.

## Quantization and numerical interface

The notebook uses post-training full-integer conversion with a representative
dataset of 1000 preprocessed training examples, int8 input/output types, and the
`TFLITE_BUILTINS_INT8` operator set. The inspected artifact uses int8 weights and
activations, with wider biases/accumulation as required by the kernels. The DSP
feature extractor still uses float arithmetic before input quantization.

| Tensor | Quantization |
|---|---|
| Model input | Per-tensor scale approximately `0.09529872239`, zero point `17` |
| Convolution weights | Per-output-channel scales; zero points all zero; Conv2D quantized dimension 0, depthwise dimension 3 |
| Convolution biases | Int32; per-channel scales and zero points zero |
| Model output | Per-tensor scale `1/256 = 0.00390625`, zero point `-128` |

The feature extractor uses the model metadata supplied during initialization:

```text
input_q = clamp(roundf(logMel / input_scale + input_zero_point), -128, 127)
yes_score = output_scale * (output_q[0] - output_zero_point)
          = (output_q[0] + 128) / 256
```

The int8 softmax representation has a step of 1/256 and a maximum representable
score of 255/256. The application dequantizes index 0; it does not perform a
three-class argmax. All successful inference scores, including low values, go to
the control task so smoothing and release/rearming receive negative evidence.
The class names/order come from the notebook's label mapping and the firmware's
interpretation of index 0. See [DSP/model input](dsp-pipeline.md#4-int8-quantization-and-model-layout)
for row ordering and logarithm/scaling details.

## Where the model and working memory reside

| Data | Location | Allocation / lifetime |
|---|---|---|
| FlatBuffer, weights, biases, serialized graph/quantization data | Internal flash, compiled from the header's `const unsigned char` array | 8496 bytes; persists across resets/reflashing until replaced |
| Tensor arena | SRAM, `alignas(16)` array in the private static `KeywordModel::Runtime` | 96 KiB = 98,304 bytes, reserved for the life of the application |
| Activations, input/output tensors, kernel scratch and prepared runtime metadata | Allocated by TFLite Micro within the arena | Planned/reused according to tensor lifetimes; actual total arena consumption is not reported by this application |
| Rolling DSP feature context | `FeatureExtractor` member array; application has static lifetime | 1220 bytes, copied into the arena's input tensor for each invocation |
| MicroProfiler event storage | Member of the private static `KeywordModel::Runtime` | 20,488 bytes in the recorded Nano build; 1024 event capacity |
| Interpreter / four-type operator resolver | Private runtime members; interpreter constructed in fixed storage during `KeywordModel::begin` | 176 / 188 bytes in the recorded build; pointers/scalars and guards add small allocations |

`tflite::GetModel` maps the compiled FlatBuffer. In the pinned runtime, constant
tensor payloads reference the model buffers; the complete blob is not copied
into SRAM for each invocation. The arena holds mutable inference state and
runtime allocations. Its 96 KiB reservation is not the size of the learned
weights, and successful `AllocateTensors()` does not establish the minimum arena
size or quantify spare arena capacity. A typical intermediate activation here
has 4960 int8 elements; tensor lifetimes and scratch requirements determine the
working set beyond parameter storage.

The recorded pre-object-refactor normal ELF confirmed these placements:

```text
Address     Size (hex)   Symbol
0x0003afc0  0x00002130   kws_1class_depthwise_int8_tflite (flash)
0x20007c00  0x00018000   tensor_arena (SRAM)
0x2001fc14  0x00005008   profiler (SRAM)
```

These are observations from the recorded build, not fixed linker addresses for
future binaries. In the current build, model flash is already included in the
332,120-byte firmware flash footprint. The arena, profiler, interpreter, resolver,
and DSP context are already included in the 209,328-byte static RAM total; do not
subtract them again from the build's 52,816-byte remainder. That remainder is not
measured free heap. See [current build verification](cpp-idioms.md#verification).

The profiler wraps its fixed event storage when full. Normal telemetry reports
whole-invocation timing; the application does not currently export a per-layer
profiling report. Its profiler RAM is separate from the tensor-arena reservation.

## Runtime initialization and invocation

`KeywordModel::instance()` returns the single noncopyable model wrapper. Its
private runtime holds the arena, resolver, profiler, tensors, and interpreter
storage with static lifetime; no separate globals expose that state.
`KeywordModel::begin` is called once during startup before capture starts;
calls after interpreter construction return the established initialization status.
It maps the compiled model,
checks schema version compatibility, registers four operator types (`CONV_2D`,
`DEPTHWISE_CONV_2D`, `MEAN`, `SOFTMAX`), placement-constructs a `MicroInterpreter`
in its statically reserved member storage, and
calls `AllocateTensors`. The eight graph operators reuse those four registrations.
It obtains tensor quantization metadata and validates int8 input/output types,
1220 input bytes, at least one output byte, and positive scales. The compiled
artifact has three output elements; initialization does not independently check
all tensor dimensions or the semantic label order.

After DSP warmup, only the audio task calls `KeywordModel::predict`. Each call
copies the latest 61 x 20 int8 feature matrix, invokes the interpreter, and
dequantizes the `yes` score. Failure increments the inference-error counter and
advances the stream generation, invalidating old detection evidence. The
application does not perform per-invocation model loading or application heap
allocation in this path.

The locked build includes CMSIS-NN convolution, depthwise-convolution, and
softmax kernels; its compile commands and linked `arm_convolve_wrapper_s8`,
`arm_depthwise_conv_wrapper_s8`, and `arm_softmax_s8` symbols confirm those paths.
MEAN is supplied by TFLite Micro's reduction implementation. CMSIS-NN implements
integer neural-network kernels; CMSIS-DSP implements the float feature pipeline.
The runtime revision and compatibility headers are pinned in
[dependencies.lock.json](../dependencies.lock.json) and documented in
[build provenance](build.md#locked-inputs-and-provenance).

Inference is nominally attempted every eight new feature rows (128 ms), subject
to backlog/readiness checks. The [final normal telemetry](rtos-final-normal.json)
reported 58.630 ms average and 59.652 ms observed maximum inference time. That
measurement includes input copy, invocation, and score decoding, not DSP or
control-task filtering. These are observed times, not a proven worst-case bound.
The [RTOS design](rtos-design.md) documents scheduling and score-queue ownership.

## Training evidence and limitations

The notebook loads the TensorFlow Datasets `speech_commands` train/validation/test
splits, maps labels to `yes`/unknown/silence, applies training time shifts up to
100 ms and volume scaling, and trains for three epochs using Adam and sparse
categorical cross-entropy. Its saved floating-point test evaluation is
approximately **93.48% overall accuracy**. This is a historical notebook result,
not a fresh evaluation of the quantized artifact or microphone deployment.
No per-class false-accept/false-reject rate is established by that aggregate.

The physical spoken-word/LED check verifies end-to-end operation. It does not
establish dataset accuracy. Full quantized-model accuracy, controlled speaker/noise
coverage, training reproducibility, and numerical feature parity remain separate
validation work. See [DSP verification coverage](dsp-pipeline.md#training-alignment-and-verification-limits).
Retraining is not claimed to reproduce the same bytes; the tracked binary and
matching header identify the actual deployed model.

## Informal recognition and possible model expansion

On October 20, 2026, the developer reported an empirical estimate of **roughly
95% keyword-recognition success during informal use** of the current model:
**1075 deployed parameter values** (992 weights and 83 biases), corresponding to
1315 total Keras parameters before conversion. This associates the reported
recognition behavior with the inspected model's small parameter count and the
current DSP/filter/control implementation.

The estimate describes the developer's experience under current operating
conditions. A labeled attempt log, sample count, speaker/noise distribution, and
false-trigger rate were not supplied with it. It is therefore an informal
recognition estimate, not a measured three-class dataset accuracy, a confidence
interval, or the saved notebook's separate 93.48% floating-point test result.
The [hardware validation record](rtos-validation.md#developer-reported-informal-recognition)
records this provenance separately from the captured telemetry.

**Working hypothesis:** increasing model capacity, for example by adding feature
channels or another depthwise/pointwise block, could potentially improve
recognition into the **upper 90% range**, with representative training data and
appropriate tuning. No larger candidate has been trained or evaluated in this
project to establish that improvement. Added parameters may help if current
errors reflect limited model capacity; data coverage, preprocessing alignment,
and detection thresholds can also determine the result.

Evaluate this hypothesis by comparing the current and larger models on the same
held-out labeled recordings and continuous-stream trials across intended
speakers/noise conditions. Report successful detections, missed keywords, and
false triggers separately, then measure the quantized candidates' inference time
and peak RAM on the Nano. Capacity expansion must preserve the application's
audio continuity and response requirements. The proposed upper-90s outcome is a
testable possibility, not a demonstrated result or a guaranteed forecast.

## Reproduce the artifact inspection

After `make setup`, the pinned schema/FlatBuffers headers are available locally.
Compile the host-only inspector and read the model without installing TensorFlow:

```sh
mkdir -p .build/model-review
c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I.build/arduino-user/libraries/Arduino_TensorFlowLite/src \
    -I.build/arduino-user/libraries/Arduino_TensorFlowLite/src/third_party/flatbuffers/include \
    scripts/inspect_model.cpp -o .build/model-review/inspect_model
.build/model-review/inspect_model models/kws_1class_depthwise_int8.tflite > .build/model-review/metadata.json
python3 -m json.tool .build/model-review/metadata.json
```

The inspector verifies FlatBuffer structure and reports the single graph's
operators, tensor shapes/types/quantization, and unique convolution weight/bias
tensor counts. The checked-in [metadata](model-metadata.json) records that output.
It is an inspection aid, not an inference runner or a universal parameter counter
for every TFLite operator. Its successful sanitized run and invalid/missing-file
rejection were checked separately from the dependency-free host test suite.

To reproduce the linked placement check after `make build`:

```sh
.build/arduino-data/packages/arduino/tools/arm-none-eabi-gcc/7-2017q4/bin/arm-none-eabi-nm \
    -S -C .build/artifacts/KWS_Arm_CortexM4F.ino.elf \
    | rg 'kws_1class_depthwise_int8_tflite$|tensor_arena$|profiler$'
```

The verification of artifact/header identity, tensor metadata, parameter counts,
and ELF placement does not run the network on labeled examples. See
[unit/regression coverage](testing.md) and [hardware evidence](rtos-validation.md)
for the different validation scopes.
