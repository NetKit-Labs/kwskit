# Zephyr project documentation

The maintained firmware has one platform: Arduino Zephyr Boards on the Nano 33
BLE Sense. Start with the [project overview](../README.md), then follow the topic
guides below. All runtime descriptions and hardware evidence here refer to Zephyr.

| Guide | Contents |
| --- | --- |
| [Build and CI](build.md) | Host prerequisites, locked dependencies, local setup, upload, artifacts, IDE flags, troubleshooting and PDF generation |
| [Zephyr runtime](zephyr-runtime.md) | Official loader, static linking, PDM configuration, memory and stack-diagnostic limits |
| [Engineering overview](engineering-overview.md) | Requirements, design decisions, source map, evidence and remaining work |
| [RTOS design](rtos-design.md) | Scheduling, thread ownership, kernel APIs, startup, synchronization and overload |
| [DSP pipeline](dsp-pipeline.md) | Exact feature dimensions/formulas, periodic Hann, packed FFT, quantization and training alignment |
| [TinyML inference](tinyml-inference.md) | Deployed graph, model identity, parameter counts, quantization, storage and training results |
| [C++ ownership](cpp-idioms.md) | Fixed-size contracts, RAII, callbacks, references, resource lifetimes and C++20 facilities |
| [Source style](source-style.md) | Language requirements, include grouping, formatters and source coverage |
| [Unit and regression tests](testing.md) | Commands, coverage, Zephyr fixtures, hardware procedures and limitations |
| [Real-time review](realtime-review.md) | Bounded work, freshness, fault handling, memory limits and timing interpretation |
| [Validation](validation.md) | Identified hardware sessions, builds, CI, cleanup verification and raw captures |
| [Model metadata](model-metadata.json) | Inspected graph, tensor shapes, quantization and parameter counts |
| [Model diagram](kws_1class_depthwise_int8.tflite.svg) | Exported network graph |
| [PDF project guide](README.pdf) | Current PDF export of the project README |

The original Zephyr session is recorded in [normal](zephyr-normal.json),
[stress](zephyr-stress.json), [restored](zephyr-restored.json) telemetry and its
[summary](zephyr-validation.json). These are dated evidence, not fresh results
for every future source change. The validation guide distinguishes new checks
from the retained session. No training or acoustic accuracy is inferred from a
count of keyword actions.
