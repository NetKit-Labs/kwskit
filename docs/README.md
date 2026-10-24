# kwskit documentation

The maintained firmware targets a 64 MHz nRF52840 Arm Cortex-M4F running
Zephyr RTOS. Start with the [project overview](../README.md), then follow the topic
guides below. All runtime descriptions and hardware evidence here refer to Zephyr.

| Guide | Contents |
| --- | --- |
| [Build and CI](build.md) | Host prerequisites, locked dependencies, local setup, upload, artifacts, IDE flags, troubleshooting and PDF generation |
| [Zephyr runtime](zephyr-runtime.md) | Official loader, static linking, PDM configuration, memory and stack-diagnostic limits |
| [Engineering overview](engineering-overview.md) | Requirements, design decisions, source map, evidence and remaining work |
| [RTOS design](rtos-design.md) | Thread priorities/ownership, startup sequence, kernel APIs, wait policies, synchronization and overload |
| [DSP pipeline](dsp-pipeline.md) | Exact feature dimensions/formulas, periodic Hann, packed FFT, quantization and training alignment |
| [TinyML inference](tinyml-inference.md) | Deployed graph, model identity, parameter counts, quantization, storage and training results |
| [C++ ownership](cpp-idioms.md) | Fixed-size contracts, RAII, callbacks, references, resource lifetimes and C++20 facilities |
| [Source style](source-style.md) | Language requirements, include grouping, formatters and source coverage |
| [Unit and regression tests](testing.md) | Commands, coverage, Zephyr fixtures, hardware procedures and limitations |
| [Real-time review](realtime-review.md) | Bounded work, freshness, fault handling, memory limits and timing interpretation |
| [Validation](validation.md) | Identified hardware sessions, builds, CI, cleanup verification and raw captures |
| [Model metadata](model-metadata.json) | Inspected graph, tensor shapes, quantization and parameter counts |
| [Model diagram](kws_1class_depthwise_int8.tflite.svg) | Exported network graph |
| [PDF project guide](README.pdf) | Export of the root `README.md`, checked for freshness in CI |

The PDF comes from the root project overview, not this documentation index.
Topic guides have no PDF counterparts; their Markdown files are authoritative.
See [PDF generation](build.md#troubleshooting-and-pdf-generation) to regenerate
the overview and check parity.

The original Zephyr session is recorded in [normal](zephyr-normal.json),
[stress](zephyr-stress.json), [restored](zephyr-restored.json) telemetry and its
[summary](zephyr-validation.json). These are dated evidence, not fresh results
for every future source change. The validation guide distinguishes new checks
from the retained session. No training or acoustic accuracy is inferred from a
count of keyword actions.

The later C++20 hardening session has its own [source/evidence summary](zephyr-realtime-validation.json)
and normal, control-stall and restored-image records linked from
[validation](validation.md#c20-real-time-hardening-validation). It includes the
lower-priority diagnostic main thread, fixed audio-batch budget and build-time
allocation policy; the original captures describe the earlier image.

The later [PDM backoff validation](validation.md#pdm-read-error-backoff-validation)
identifies the driver patch, actual-worker fault test and the latest board image.
Its [source/evidence summary](zephyr-pdm-validation.json) links the normal,
control-stall and restored-image captures.

The subsequent [spoken-keyword/physical LED check](validation.md#spoken-keyword-and-physical-led-follow-up)
reuploaded the same current image. Its [summary](zephyr-keyword-validation.json)
and [raw capture](zephyr-keyword.json) record 13 new actions, consistent reported
LED state and the user's confirmation that the physical LED toggled on “yes.”

The sketch and checkout now share the `kwskit` name. The
[rename validation](kwskit-name-validation.json) and
[normal capture](kwskit-name-normal.json) identify the rebuilt/uploaded image;
earlier hardware records retain their original sketch filenames and hashes.
