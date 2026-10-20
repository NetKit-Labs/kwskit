# Periodic Hann window validation

This record describes the periodic-window build before the subsequent
[C++ idioms review](cpp-idioms.md). The firmware uses the training notebook's periodic Hann convention:

```text
w[n] = 0.5 - 0.5 * cos(2*pi*n / FRAME_SIZE)
FRAME_SIZE = 512; n = 0..511
```

The frame length, overlap, mel filters, quantization parameters, model bytes,
and inference scheduling are unchanged. The [DSP pipeline](dsp-pipeline.md)
describes the current processing stages.

## Host regression

On October 20, 2026, `make format-check` and all thirteen entries in `make test`
passed. The actual extractor is exercised with the scalar host DSP backend,
including failure paths, warmup/reset, inference cadence, and independent objects.
The window applied to samples is inspected through that backend. Assertions check
its sum (256), first/quarter-cycle/midpoint values, and nonzero final coefficient.
These assertions were verified to fail before the denominator change and pass
after it, independently of the quantized-feature digest.

The frozen reference was produced using the procedural extractor from
`a2c3c0d` with periodic Hann applied, the same deterministic PCM sequence, and
the same scalar host backend. The maintained extractor matched all 84,180 bytes
for silence, tone, noise, and three quantization scales, including saturation.
The quantized-feature SHA-256 is:

```text
c14a17d8294d72bc45082f4bd525fd15a93f77390b5e05658a9e31f969327d81
```

This checks the window convention and feature regression. It does not establish
complete TensorFlow/CMSIS numerical equivalence or a recognition accuracy rate.

## Firmware build

`make build JOBS=4` passed with 331,936 bytes flash and 209,320 bytes static RAM,
the same footprint as the preceding object-based build. The model blob, mel
coefficients, dependency pins, and task/ring/arena capacities are unchanged.

## Nano regression

The 20-second [control-stall capture](periodic-hann-stress.json) passed with the
1500 ms test-only delay: four score drops and nine stale-result rejections,
zero reported sample/buffer loss, and zero inference/notification errors.
Audio/inference counters advanced. The ring peaked at 768 of 8192 samples;
reported unused task stack was 6776 bytes for audio and 4992 for control.
The script rebuilds and uploads normal firmware after the stress capture.

The restored normal BIN is 332,532 bytes including image layout.
Its SHA-256 is `7e4f9916226a2751ee0aeb66888a9f28e8bca0e26929616cb5ac98b8b97c0913`.
The build manifest matches all maintained firmware files.

The 60-second [normal-firmware capture](periodic-hann-normal.json) passed after
restoration. Audio and inference counters advanced, with zero reported sample
loss, missed buffers, inference/notification errors, score drops, or stale
results. Final cumulative DSP average/maximum was 2479/2639 us; inference was
58593/58625 us; the processing-step maximum was 61227 us. The ring peaked at
768 samples and unused stack watermarks were 6776/8192 bytes for audio and
4976/6144 for control.

The final cumulative trigger count was 21, with alternating logical LED states
in the recording. The developer repeated spoken "yes" attempts and confirmed
that the built-in LED toggles. These are functional results, not an accuracy
measurement or a labeled attempt count. Counters/timing are cumulative since
boot and can include activity before the host capture. Normal firmware was restored after this run.
