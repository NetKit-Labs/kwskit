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

| Data | Location / lifetime |
| --- | --- |
| Serialized model, weights, biases and metadata | 8496-byte constant compiled into internal flash |
| Tensor arena | Fixed 65,536-byte aligned array in the static model runtime; recorded allocation use 12,500 bytes |
| Activations, input/output tensors, kernel scratch and runtime metadata | Allocated within the arena and reused according to tensor lifetimes |
| Rolling feature context | 1220-byte member array of the audio-owned extractor; copied to model input per invocation |
| Interpreter and four-type resolver | Private runtime members; interpreter placement-constructed in fixed aligned storage |

`tflite::GetModel` maps the compiled FlatBuffer. Constant tensor payloads reference
its flash-resident buffers; the whole model is not copied to RAM per invocation.
An intermediate activation has 4960 int8 elements; mutable working memory also
includes scratch and prepared metadata. The model's parameter payload, serialized
size and arena use measure different things.

The latest validated Zephyr image uses 206,824 bytes sketch flash and 118,096 bytes
sketch RAM. The arena, stacks and DSP context are already included; do not subtract them
again from the 12,976-byte sketch-region remainder. This remainder is not a free
system heap measurement. The measured allocation does not establish a minimum
safe arena size for another model. No per-layer profiler is enabled; telemetry
measures complete invocation time. See [runtime memory](zephyr-runtime.md).

## Runtime initialization and invocation

`KeywordModel::instance()` returns the single noncopyable model wrapper. Its
private runtime holds the arena, resolver, tensors, and interpreter
storage with static lifetime; no separate globals expose that state.
`KeywordModel::begin` is called once during startup before capture starts;
calls after interpreter construction return the established initialization status.
It maps the compiled model,
checks schema version compatibility, registers four operator types (`CONV_2D`,
`DEPTHWISE_CONV_2D`, `MEAN`, `SOFTMAX`), placement-constructs a `MicroInterpreter`
in its statically reserved member storage, and
calls `AllocateTensors`. The eight graph operators reuse those four registrations.
It obtains tensor quantization metadata and validates int8 input/output types,
1220 input bytes, at least one output byte, finite positive scales, and int8
input/output zero points and nonnull tensor data. Prediction rejects nonfinite or out-of-range probabilities. The compiled
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
[build provenance](build.md#single-supported-target-and-locked-inputs).

Inference is nominally attempted every eight new feature rows (128 ms), subject
to backlog/readiness checks. The [recorded Zephyr normal telemetry](zephyr-normal.json)
reported 54.994 ms average and 55.054 ms observed maximum inference time. That
measurement includes input copy, invocation, and score decoding, not DSP or
control-task filtering. These are observed times, not a proven worst-case bound.
The [RTOS design](rtos-design.md) documents scheduling and score-queue ownership.

## Training evidence and limitations

The notebook loads the TensorFlow Datasets `speech_commands` train/validation/test
splits, maps labels to `yes`/unknown/silence, applies training time shifts up to
100 ms and volume scaling, and trains for three epochs using Adam and sparse
categorical cross-entropy. Its saved Colab output reports these floating-point
results for the small model (1315 total Keras parameters):

| Evaluation | Accuracy | Loss |
| --- | --- | --- |
| Final training epoch | 98.02% | 0.0620 |
| Final validation epoch | 97.90% | 0.0522 |
| Held-out test evaluation | 93.48% | 0.1853 |

These are historical notebook results, not a fresh evaluation of the quantized
artifact or microphone deployment. The final validation and training accuracies
do not replace the separate held-out test result.
No per-class false-accept/false-reject rate is established by that aggregate.

The physical spoken-word/LED check verifies end-to-end operation. It does not
establish dataset accuracy. Full quantized-model accuracy, controlled speaker/noise
coverage, training reproducibility, and numerical feature parity remain separate
validation work. See [DSP verification coverage](dsp-pipeline.md#training-alignment-and-verification-limits).
Retraining is not claimed to reproduce the same bytes; the tracked binary and
matching header identify the actual deployed model.

## Model expansion

More channels or another depthwise/pointwise block could increase capacity, but
no larger candidate has been trained and evaluated here. Compare candidates on
the same labeled held-out recordings and continuous streams, reporting misses
and false triggers separately. Measure quantized accuracy, inference time and
arena use on the Nano. Data coverage, microphone conditions, preprocessing,
quantization and detection thresholds can matter as much as parameter count;
no accuracy improvement is guaranteed by adding parameters.

## Reproduce the artifact inspection

After `make setup`, the pinned schema/FlatBuffers headers are available locally.
Compile the host-only inspector and read the model without installing TensorFlow:

```sh
mkdir -p .build/model-review
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
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
.build/arduino-data/packages/zephyr/tools/arm-zephyr-eabi/1.0.1/bin/arm-zephyr-eabi-nm \
    -S -C .build/zephyr-artifacts/KWS_Arm_CortexM4F.ino.elf \
    | rg 'kws_1class_depthwise_int8_tflite$|KeywordModel::instance\(\)::storage$'
```

The official compiler directory is selected by Arduino's package manager. The
arena is a member of the private runtime storage, rather than a standalone symbol.
Linker addresses can change across images and should not be hardcoded.

The verification of artifact/header identity, tensor metadata, parameter counts,
and ELF placement does not run the network on labeled examples. See
[unit/regression coverage](testing.md) and [hardware evidence](validation.md)
for the different validation scopes.

The shared compiler header defines `TF_LITE_STRIP_ERROR_STRINGS` for TFLite
translation units. This prevents normal runtime error reporting from reaching
blocking Arduino Serial writes. Application initialization messages and numeric
inference-error counters remain; fatal framework assertions are still a platform
fault-handling limitation documented in the [real-time review](realtime-review.md).
