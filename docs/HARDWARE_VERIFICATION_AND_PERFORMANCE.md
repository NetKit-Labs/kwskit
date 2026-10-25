# Hardware Verification and Performance Report — KWSKit

**Target:** Nano 33 BLE Sense / nRF52840, **64 MHz Cortex-M4F**, Arduino Zephyr Boards 1.0.0  
**Evidence review:** 2026-10-25 (retained hardware sessions and CI; **no new hardware execution**)  
**Scope:** Live PDM acquisition, DSP and INT8 inference, RTOS scheduling, detection-to-LED function, memory and real-time diagnostics.

## 1. Verification basis and design

The firmware acquires **16 kHz** mono PCM in **256-sample / 16 ms** blocks. The PDM producer runs at priority 3; audio worker at 4; control worker at 5; best-effort telemetry in main at 6. An **8192-sample ring**, finite audio batch budget, eight-entry score queue, sequence/generation/age checks and bounded application allocations protect work isolation. The **64 KiB tensor arena** and fixed worker stacks are initialized before acquisition. See [engineering overview](engineering-overview.md), [RTOS design](rtos-design.md), [real-time review](realtime-review.md).

## 2. Dated hardware evidence

| Evidence | Recorded result | Interpretation |
| --- | --- | --- |
| Initial normal Zephyr capture (2026-10-22) | Requested **60 s**, **31 records**, about **15,999.7 samples/s**, zero reported sample/event loss and inference errors | Hardware telemetry in [normal JSON](zephyr-normal.json), described in [validation](validation.md) |
| C++20 + PDM-backoff normal build | Requested **60 s**, **31 records**; approximately 16 kHz progression; zero reported dropped samples, missed buffers, inference/DSP errors, score drops, stale events | [PDM normal JSON](zephyr-pdm-normal.json), [build/source manifest](zephyr-pdm-validation.json) |
| Control-stall injection, same build | Requested **25 s**, **13 records**, **1500 ms** injected control stall; **4 score drops** and **9 stale rejections** were intentionally observed, without reported acquisition/inference failures | Overload rejection behaved as designed; not a zero-event-loss test. [Stress JSON](zephyr-pdm-stress.json) |
| Normal restored after stress | Requested **30 s**, **16 records**, sampling/inference continued without reported loss/error | [Restored JSON](zephyr-pdm-restored.json) |
| Spoken-keyword follow-up (2026-10-22) | Requested **120 s**, **60 records**; rate over fresh records **16,000.94 samples/s**; **13** new confirmed actions; reported LED parity consistent, and user confirmed LED toggle on “yes”; **ring max 768/8192** | Functional end-to-end confirmation, **not** 13 labeled recognition trials or an accuracy estimate. [Follow-up manifest](zephyr-keyword-validation.json), [raw telemetry](zephyr-keyword.json) |
| Later renamed-sketch image | Rebuilt/uploaded image: **206,824 B** sketch flash, **118,096 B** sketch RAM; requested **30 s** normal capture, **16 records** and zero reported losses/errors | [Renaming evidence](kwskit-name-validation.json), [capture](kwskit-name-normal.json). It has a **different binary SHA** from the spoken-keyword image; no new spoken-keyword confirmation claimed |

### Timing and memory (recorded, not worst-case bounds)

The [engineering review](engineering-overview.md) reports a typical **~2.217 ms DSP step** and **~54.994 ms inference**, with **8 × 2.217 + 54.994 = 72.730 ms** estimated work per **128 ms nominal decision interval**. This *work accounting* excludes other CPU/scheduler overhead and is **not measured CPU utilization, an interrupt-latency bound or worst-case execution time**. The stored PDM captures contain `dsp_us`, `infer_us`, `gap_max_us` and `audio_age_us` fields to audit observed performance.

| Memory item | Recorded value |
| --- | ---: |
| Renamed current sketch flash footprint | **206,824 / 557,056 B** available sketch region |
| Sketch RAM footprint (loader-defined region) | **118,096 / 131,072 B** |
| Remaining sketch RAM region | **12,976 B** (not a measurement of free heap) |
| Tensor arena reserved / measured used | **65,536 B / 12,500 B** (recorded model run) |
| Stack-watermark diagnostic | **Unavailable** with stock loader; `0/0` is not 0 bytes of headroom |

Source: [Zephyr runtime/memory](zephyr-runtime.md), [validation](validation.md). Application-level loss counters do not capture every failure in the upstream PDM driver/USB/loader.

## 3. Host and CI verification

`make test` runs **12 unit + 4 additional regression entries** with ASan/UBSan; CI also runs formatting, pinned-core firmware cross-build and sketch import audit. The separate `make test-pdm-driver` gate verifies the staged PDM worker's **1 ms error backoff** and queue/partial-read behavior using host fault injection. These are not physical microphone-error injections. See [test inventory](testing.md), [PDM compatibility](../build-support/pdm/README.md), [CI definition](../.github/workflows/ci.yml).

Reproduce on connected board after the documented `make setup` and flash:

```sh
make test
make test-pdm-driver
make build
make flash PORT=<serial-port>
python3 tests/hardware_capture.py --port <serial-port> --seconds 60 \
  --allow-unavailable-stack-metrics --output .build/normal.json
python3 scripts/rtos_stress.py --port <serial-port> --seconds 25 \
  --output .build/stress.json
```

## 4. Disposition and limits

**Supported:** observed real-board continuous acquisition/inference, deliberate stale/overflow handling during control stall, memory/build provenance and a user-observed keyword-to-LED action on an identified earlier image. **Not established:** acoustic recognition accuracy/false-trigger rate (attempt count/labels were not retained), formal WCET, whole-system CPU utilization, stack watermarks, end-to-end acoustic-onset latency, cold power cycling, hours/days endurance, physical microphone fault recovery, or production-grade real-time certification. See [limits and interpretation](realtime-review.md).

No measurements or green CI results are newly claimed by this documentation-only report.
