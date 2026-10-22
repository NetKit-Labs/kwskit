# Zephyr validation and evidence

## Identified hardware session: October 22, 2026

The retained session used Arduino Zephyr Boards 1.0.0, static link mode, official
GCC 14.3.0 and the pinned model/DSP libraries. Its packaged normal application
SHA-256 was:

```text
f22f4a341ac33cc33df7cc613cbf717b60734c514ca114b263a4833db5985928
```

The image used 206,032 bytes sketch flash and 118,096 bytes sketch RAM. The
stock loader reserves 557,056 flash bytes and 131,072 RAM bytes for the sketch.
These are sketch budgets, not whole-board free memory measurements. The
64 KiB tensor arena used 12,500 bytes at allocation, printed during startup.

| Check | Observed result |
| --- | --- |
| Initial upload | Arduino CLI/bossac installed the 275,720-byte official loader and 206,032-byte packaged sketch; subsequent USB/KWS operation observed |
| [Normal capture](zephyr-normal.json) | 60-second requested capture, 31 records, approximately 15,999.7 samples/second, no reported audio loss, inference errors, event loss or stale scores |
| Spoken “yes” | 16 cumulative confirmed actions; user independently confirmed physical LED toggling |
| Monitor closed/reopened | Sample/inference counters continued without reported loss; a buffered record preceded fresh output |
| [Control-stall capture](zephyr-stress.json) | 25-second requested capture, 14 records, 1500 ms injected stall, four expected event drops and nine stale-score rejections, no reported audio/inference failure |
| [Restored normal capture](zephyr-restored.json) | Normal image reuploaded and verified for 30 seconds, 16 records, 492,032 samples and 233 inferences cumulatively, no reported failures |

The normal run ended at 2,343,424 samples and 1,137 inferences. Cumulative DSP
average/maximum was 2.217/5.249 ms, inference 54.994/55.054 ms and processing-step
maximum 57.342 ms. Ring high-water was 768 of 8192 samples; callback gap maximum
was 16.022 ms. Event delivery average/maximum was 9.503/13.855 ms. Timings are
observations for exercised paths, not worst-case bounds or acoustic-onset latency.
The [summary](zephyr-validation.json) preserves identity and final counters.

## Remote verification

[GitHub Actions run 39785916551](https://github.com/NetKit-Labs/KWS_Arm_CortexM4F/actions/runs/39785916551)
passed the Zephyr firmware build, formatting and shared host tests for commit
`48b774c`. Its Zephyr firmware/provenance artifact was uploaded. That run predates
the single-target cleanup; it is not evidence that a later commit ran remotely.
The maintained workflow now contains only the Zephyr firmware target.

## Single-target cleanup verification

The single-target cleanup passed `make test-unit` (12 entries),
`make test-regression` (16 entries, including nine build-tooling cases),
`make format-check`, the simplified Zephyr setup, and the Nano cross-build.
Legacy-only test fixtures were removed; capture uses Zephyr IRQ fixtures, the
coordinator fixture covers startup/final-action/overflow behavior, and a new
serial telemetry unit checks disconnected and backpressured output.

The rebuilt packaged normal image is byte-for-byte identical to the hardware
session image identified above: 206,032 bytes, with the same SHA-256. Flash and
RAM remain 206,032/118,096 bytes. No fresh upload or speech session is claimed
for the cleanup; the matching executable ties it to the retained hardware runs.
The model and mel coefficients are unchanged.

All 130 local Markdown file/anchor links across 14 guides were checked. The root
overview and topic guides describe the single Zephyr runtime. Historical platform
records were removed from the active documentation, and the refreshed four-page
PDF was rendered and all pages visually inspected. A source/docs audit found no
retired-platform API, target-selection code, or documentation references.

## Limits and reproducibility

Run [unit/regression checks](testing.md), follow [build/upload](build.md), then
repeat normal, disconnected-monitor and control-stall checks for relevant
runtime changes. Preserve raw captures and source manifests. Counters include
operation before capture; 16 actions are not 16 labeled trials or an accuracy
rate. A physical LED observation establishes functional correspondence only.

The stock loader cannot measure application stack headroom. No peak system heap,
cold power-cycle, hours-to-days soak, calibrated acoustic accuracy or broad
peripheral/allocation/brownout fault campaign is claimed. Training notebook
results and inspected model metadata are separate from microphone deployment
accuracy. See [real-time limits](realtime-review.md) and
[training evidence](tinyml-inference.md#training-evidence-and-limitations).
