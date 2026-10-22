# Zephyr engineering overview

This application continuously classifies a rolling second of microphone audio
on a 64 MHz Cortex-M4F. Samples arrive every 16 ms while inference takes roughly
55 ms. Acquisition therefore needs independent scheduling and buffering, and
control must reject results that become obsolete during a delay.

## Requirements and decisions

| Requirement | Implementation and tradeoff |
| --- | --- |
| Preserve audio through bounded processing delays | Priority-3 PDM callback publishes into an 8192-sample ring; full rings reject new samples and report loss |
| Bound acquisition work | At most one 512-byte PCM block per callback; no DSP, model, actions or serial output |
| Isolate slow work | Audio priority 4, control priority 5, telemetry in main at priority 6; score sends never wait |
| Avoid cross-thread buffer lifetime problems | Eight-entry Zephyr queue copies values, never audio/tensor pointers |
| Prevent stale actions | Generation, sequence and dual-clock age checks; final scheduler-locked check immediately before GPIO |
| Avoid isolated/repeated detections | EMA, raw/smoothed thresholds, consecutive evidence, release hysteresis and cooldown |
| Make evidence reviewable | Locked dependencies, actual-module host tests, source manifests, CI and dated telemetry |

## Source map

| Source | Responsibility |
| --- | --- |
| [kws_pipeline.cpp](../firmware/kws_pipeline.cpp) | Zephyr resources, startup, thread bodies, generation handling and action serialization |
| [audio_capture.cpp](../firmware/audio_capture.cpp) | Callback ownership, bounded PDM reads and arrival/loss snapshots |
| [audio_queue.h](../firmware/audio_queue.h) | Atomic single-producer/single-consumer ring |
| [audio_pipeline.cpp](../firmware/audio_pipeline.cpp) | Audio loss recovery, readiness, backlog deferral and timing |
| [feature_extractor.cpp](../firmware/feature_extractor.cpp) | Frame overlap, periodic Hann, FFT, mel/log and input quantization |
| [fft_power.h](../firmware/fft_power.h) | Separate DC/Nyquist and interior complex power bins |
| [keyword_model.cpp](../firmware/keyword_model.cpp) | Model schema/operators, fixed arena, tensor validation and score decoding |
| [keyword_event.h](../firmware/keyword_event.h) | Freshness, continuity and temporal-evidence invalidation |
| [keyword_filter.h](../firmware/keyword_filter.h) | Smoothing, qualification, latch and cooldown |
| [device_control.cpp](../firmware/device_control.cpp) | Initialized LED and toggle action |
| [telemetry.cpp](../firmware/telemetry.cpp) | Fixed formatting buffer and limited Serial requests in best-effort main |

The static application owns noncopyable stateful modules. Audio exclusively
uses DSP/model state after initialization; control owns detection/LED. The
priority-6 Arduino main thread owns best-effort telemetry and optional stack scans.
References describe required dependencies and typed arrays carry extents across
interfaces. Pointer/context adapters stay confined to platform API boundaries.
See [ownership](cpp-idioms.md) and [scheduling](rtos-design.md).

## Evidence and remaining work

The [original Zephyr normal capture](zephyr-normal.json) records approximately 16 kHz
sampling, zero reported losses/errors, 16 confirmed actions and physical LED
confirmation. Its average DSP/inference work over eight hops is approximately
`8 * 2.217 + 54.994 = 72.730 ms` per nominal 128 ms decision interval.
This estimate excludes other work and is not measured CPU utilization or WCET.
A [control stall](zephyr-stress.json) demonstrates score loss/stale rejection
while audio continues. [Validation](validation.md) distinguishes those recorded
hardware results from the cleanup build and current host/CI checks.

The [latest source/capture summary](zephyr-pdm-validation.json) identifies the
206,824-byte image with C++20 hardening and PDM read-error backoff. Its normal,
control-stall and restored-image checks passed; the speech/LED observation above
belongs to the original image and was not repeated for this revision.

The stock loader gives the sketch 128 KiB RAM. The arena reserves 64 KiB and used
12,500 bytes in the recorded model run; fixed application stacks are 8192/6144
bytes. Stack watermarks and peak system heap remain unmeasured. Cold-start,
extended endurance, fault recovery and calibrated acoustic/model accuracy need
separate trials. [DSP](dsp-pipeline.md), [inference](tinyml-inference.md),
[testing](testing.md) and [real-time limits](realtime-review.md) expand these topics.
