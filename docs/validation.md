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

[GitHub Actions run 39791624039](https://github.com/NetKit-Labs/KWS_Arm_CortexM4F/actions/runs/39791624039)
completed successfully for the Zephyr-only cleanup commit `2fcbb09`. The Zephyr
firmware build, complete host regression suite, formatting and firmware/provenance
artifact upload passed. The maintained workflow contains only the Zephyr firmware
target. This identified run is evidence for that commit; later changes need their
own checks.

[Application-hardening run 39802129083](https://github.com/NetKit-Labs/KWS_Arm_CortexM4F/actions/runs/39802129083)
also completed successfully for `097e520`, covering the C++20 safety flags,
object audit, firmware artifact and host/style gates.

[PDM-backoff run 39804902644](https://github.com/NetKit-Labs/KWS_Arm_CortexM4F/actions/runs/39804902644)
completed successfully for `452e738`. Source formatting, all host regressions,
the actual-driver fault gate, the Zephyr cross-build/object audit and firmware
artifact upload passed. It verifies the source of the latest recorded Nano image;
later documentation changes do not constitute new hardware tests.

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

## Documentation and source consistency review

The October 22 follow-up review checked all 123 tracked paths and readable file
contents, plus freshly extracted PDF text, for retired-platform names/APIs.
None were present in the maintained project. The RTOS guide now includes the
worker lifecycle, startup sequence diagram, ownership and synchronization wait
policies. Source comments identify the actual PDM thread callback context and
fixed kernel-object lifetimes.

`make format-check`, all 16 `make test-regression` entries (including nine
build-tooling cases), the Zephyr cross-build, JSON parsing and all 131 local
Markdown links/anchors passed. Formatting is consistent across the maintained
C++, Python and notebook sources. The packaged application still matches the
hardware-session SHA-256 and flash/RAM sizes above. This review changed comments,
documentation and an unused include; no fresh upload or hardware test was needed
or performed.

## C++20 real-time hardening validation

The subsequent hardening build changes runtime behavior and has a new packaged
application SHA-256:

```text
1b9aaf930585b028a327ee1e7dc02b5f42d8a52b53fe21ff9ccce563cc2f73a7
```

Arduino reports 206,808 bytes sketch flash and 118,096 bytes sketch RAM. The model
and mel tables are unchanged. The [source/evidence summary](zephyr-realtime-validation.json)
records the staged source hashes, C++20 safety flags and capture identities.
The official cross-build and its 115-import sketch audit passed. Formatting and
all 16 host regression entries passed, including ten build-tooling cases and new
DSP-error, timing-rebase, finite-batch and diagnostic-isolation checks.

| Hardware check | Observed result |
| --- | --- |
| [Normal capture](zephyr-realtime-normal.json) | 60 seconds requested, 31 records, approximately 16 kHz sample progress; zero reported sample loss, missed buffers, inference/DSP errors, score drops or stale scores |
| [Control-stall capture](zephyr-realtime-stress.json) | 25 seconds requested, 13 records, injected 1500 ms stall; four expected score drops and nine stale rejections, no reported audio or inference/DSP failure |
| [Restored normal capture](zephyr-realtime-restored.json) | Normal image restored, 30 seconds requested, 16 records, acquisition/inference progress and zero reported losses/errors |

An initial live check rejected a buffered record generated before the first
microphone block. The reporter now starts its two-second epoch on its first
post-initialization call; a unit test covers that interval, and the normal
hardware check was repeated successfully. No checks were disabled to accept the
startup record. The original speech/LED observation above is historical; no new
spoken keyword or physical LED observation is claimed for this build.

The updated four-page PDF was rendered and every page visually checked. See the
[real-time review](realtime-review.md) for the allocation boundary, Serial wait
isolation and original upstream PDM error-retry limitation. The driver follow-up
below addresses that retry loop. These checks
establish exercised behavior, not whole-system hard real-time safety.
All 139 local Markdown file/anchor links and the documentation JSON parsed
successfully; recorded firmware source hashes match the maintained files.

## PDM read-error backoff validation

The later driver fix preserves the application source above and adds a 1 ms
backoff in the staged PDM worker's read-error path. The packaged application is
206,824 bytes, with 118,096 bytes sketch RAM reported by Arduino. Its SHA-256 is:

```text
b9701db4855a56faf4576d92086557682da0f7bcef8bbd8350079ea249dd8306
```

The compiler dependency file identifies `.build/pdm-realtime/PDM/src/PDM.cpp`
as the PDM source used. The official core's original source remains intact.
Normal/stress manifests include the staged driver hashes. The full cross-build,
sketch object audit, formatting, 16 host regression entries and eleven build-tooling
cases passed. `make test-pdm-driver` separately passed: the unpatched actual worker
failed the retry-delay check, and the staged worker passed transient/persistent
read errors, full-queue slab release and no-progress callback cases.

The original tight retry loop is therefore addressed in the maintained build;
the earlier hardening summary describes the pre-backoff image. These fault tests
use host fixtures and do not inject physical microphone failures or measure
device-call WCET. The [driver note](../build-support/pdm/README.md) records the
patch scope and test procedure.

The [source/evidence summary](zephyr-pdm-validation.json) identifies the final
application and staged driver hashes. All three board checks passed:

| Hardware check | Observed result |
| --- | --- |
| [Normal capture](zephyr-pdm-normal.json) | 60 seconds requested, 31 records, approximately 16 kHz sample progress; zero reported sample loss, missed buffers, inference/DSP errors, score drops or stale scores |
| [Control-stall capture](zephyr-pdm-stress.json) | 25 seconds requested, 13 records; four expected score drops and nine stale rejections with no reported audio or inference/DSP failure |
| [Restored normal capture](zephyr-pdm-restored.json) | Normal image restored, 30 seconds requested, 16 records, continuing acquisition/inference and zero reported losses/errors |

The four-page overview PDF was refreshed and all pages visually inspected.
All 156 local file/anchor links across 15 guides passed; application, driver and
build-tooling source hashes match the recorded evidence summary.
No new spoken keyword/physical LED check, cold power-cycle, extended soak or
physical microphone fault injection is claimed for the follow-up image.

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
