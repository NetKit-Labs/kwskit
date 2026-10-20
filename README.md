# TinyML Keyword Spotting on ARM Cortex-M

Lightweight keyword spotting (KWS) system for microcontrollers using TensorFlow Lite Micro, CMSIS-DSP, and CMSIS-NN.  
Detects a single keyword ("yes") from 1-second audio windows with low latency.

---

## Metrics & Performance Summary

| Metric | Value |
|--------|-------|
| Model type | CNN (6 Conv layers + GAP + Softmax) |
| Parameters | ~1,000 |
| Input | 61 frames × 20 log-mel coefficients (~1s audio) |
| Quantization | int8 |
| Inference time | ~60 ms on Cortex-M4F 64 MHz |
| Training accuracy | ~90%+ (3 epochs) |
| Memory footprint | Very small, fits on low-power MCUs |

---

## Highlights

- Approximately 1,000 parameter CNN  
- ~60 ms inference on Cortex-M4F @ 64 MHz  
- int8 quantized model  
- CMSIS-NN accelerated CNN  
- CMSIS-DSP accelerated feature extraction  
- Designed for low-power Cortex-M MCUs  

---

## Project Overview

Keyword spotting is a lightweight speech recognition system for detecting specific trigger words.  

This project implements single-keyword spotting using the Google Speech Commands dataset, deployed on an ARM Cortex-M4F MCU.

### Classes

- `keyword` ("yes")  
- `unknown speech`  
- `silence`  

Effectively acts as binary classification:  
`keyword vs everything else`

The keyword "YES" is chosen for its three phonemes, improving single-word detection reliability.

---

## Model Summary

| Feature | Value |
|---------|------|
| Model type | Convolutional Neural Network (CNN) |
| Parameters | ~1,000 |
| Layers | 6 convolution layers |
| Pooling | Global Average Pooling |
| Output | Softmax |
| Quantization | int8 |
| Input | Log-Mel Spectrogram |
| Framework | TensorFlow / TensorFlow Lite Micro |

---

## Input Features

### Audio Parameters

| Parameter | Value |
|-----------|------|
| Sample Rate | 16 kHz |
| Frame Size | 512 samples |
| Frame Duration | 32 ms |
| Hop Size | 256 samples |
| Hop Duration | 16 ms |
| Overlap | 50% |

### Spectrogram Dimensions

61 time frames × 20 log-mel coefficients (~1 second of audio)

---

## Model Architecture

- 3 convolutional stages: Conv2D → DepthwiseConv2D  
- Global Average Pooling  
- Softmax output

```
Input Spectrogram
    ↓
Conv2D → DepthwiseConv2D
    ↓
Conv2D → DepthwiseConv2D
    ↓
Conv2D → DepthwiseConv2D
    ↓
Global Average Pooling
    ↓
Softmax
```

Stacked layers increase the receptive field, capturing long-range phonetic patterns.

![Receptive Field](1sec_timing.png)

---

## Embedded Deployment

### Hardware

- ARM Cortex-M4F @ 64 MHz  
- Hardware FPU for DSP acceleration  

### Libraries

- CMSIS-DSP: FFT, vector/matrix math  
- CMSIS-NN: accelerated CNN kernels  

TensorFlow Lite Micro automatically invokes CMSIS-NN for optimal performance.

---

## Feature Extraction Pipeline

```c
arm_mult_f32(audioFrame, hannWindow, windowed, FRAME_SIZE);
arm_rfft_fast_f32(&fft_inst, windowed, fftOut, 0);

// CMSIS fast real FFT stores DC and Nyquist in its first two floats.
mag[0] = fftOut[0] * fftOut[0];
for (int i = 1; i < FRAME_SIZE / 2; ++i) {
    mag[i] = fftOut[2*i] * fftOut[2*i] + fftOut[2*i+1] * fftOut[2*i+1];
}
mag[FRAME_SIZE / 2] = fftOut[1] * fftOut[1];

arm_mat_mult_f32(&melMat, &magVec, &melOut);

for (int m = 0; m < NUM_MEL; m++) {
    float log_mel = logf(melRowTmp[m] + 1e-6f);
    int32_t q = (int32_t)roundf(log_mel / input_scale + input_zero_point);
    quantized_mel_row[m] = (int8_t)constrain(q, -128, 127);
}
```

Manual power spectrum ensures consistency between training and MCU pipeline.

---

## Performance

### Training

| Metric | Value |
|--------|-------|
| Accuracy (3 epochs) | ~90%+ |
| Dataset | Google Speech Commands |

### Inference

| MCU | Inference Time | Parameters |
|-----|----------------|-----------|
| Cortex-M4F 64MHz | ~60 ms | ~1,000 |

Optimization could reduce inference to 20–30 ms.

---

## Benchmark & Comparison

| Model | Dataset | Parameters | Inference (ms) | Accuracy | MCU |
|-------|---------|-----------|----------------|---------|-----|
| TinyML KWS (this project) | Google SC | ~1,000 | 60 | ~90%+ | Cortex-M4F 64MHz |
| TFLM CNN Example | Google SC | ~8,000 | 120 | 92 | Cortex-M4F 72MHz |
| ARM CMSIS-NN Ref | Google SC | ~2,500 | 45 | 88 | Cortex-M4 80MHz |
| KeywordSpotting-Small | Google SC | ~10,000 | 150 | 95 | STM32F746 216MHz |

TinyML KWS is ultra-lightweight and maintains real-time operation.

---

## Batch vs Streaming Inference

- Batch mode: ~1 s latency  
- Current firmware: rolling ~1 s feature window, decisions every 8 hops (~128 ms), with confidence filtering

Circular buffers can reduce DSP overhead and improve streaming latency.

---

## Silence Handling

Limited dataset silence → weak correlation with real silence.  

Solution: augment silence data and retrain.

---

## Future Work

1. Tune streaming detection thresholds using recorded positive and negative examples
2. Multi-keyword detection (5–10 keywords)  
3. TinyLLM voice interface with flexible commands:

```
Robot, get me a black coffee
Robot, I want a coffee
Robot, I'll take my coffee now
```

---

## Model Visualization

Generated using Netron:

![Model Architecture](kws_1class_depthwise_int8.tflite.png)

---

## License

MIT License


## Updated firmware and tests

The maintained sketch is `firmware/KWS_Arm_CortexM4F.ino`. For Arduino IDE,
copy the contents of `firmware/` into a sketch folder named `KWS_Arm_CortexM4F`.
Select **Arduino Nano 33 BLE**. The empty root-level `.ino` file is not the firmware.

The interrupt callback drains bounded PDM reads into an 8192-sample ring (512 ms).
The main loop consumes complete 256-sample hops and preserves 50% frame overlap
across inferences. Acquisition starts after model initialization. Overflow drops
new samples, counts the loss, and resets the feature window before resuming
classification. The queue can absorb bounded stalls; it cannot compensate for
sustained processing slower than acquisition.

Class 0 is `yes`, matching the training notebook. An EMA (weight 0.5) must reach
0.75 with raw confidence at least 0.8 on two successive decisions. A confirmed
keyword toggles `LED_BUILTIN` (initially off). The filter rearms below 0.4 and
requires at least 1500 ms between triggers. These starting thresholds require
validation with real speech and background noise; host tests do not establish
false-positive rates.

At 115200 baud, the serial monitor receives telemetry every two seconds:

- `samples`: cumulative samples delivered by PDM; should keep increasing during inference.
- `dropped`: ring overflow samples; `missed_buffers`: callbacks that lost any samples.
- `queue_max`: peak queued samples (capacity 8192).
- `gap_max_us`: largest callback interval; `audio_age_us`: time since the last callback, exposing acquisition stalls that ring counters alone cannot detect.
- `dsp_us`, `infer_us`, `process_us`: average/maximum execution time in microseconds. Processing includes consumption, DSP, and any inference in that iteration.
- `response_us`: last/maximum delay from the end of the first high-confidence audio window to the LED action. It includes filtering and processing delay. It is an estimate at callback resolution (~16 ms), not acoustic word-onset latency; measuring that requires a known external audio stimulus.
- `inferences`, `errors`, `p_milli`, `triggers`, `led`: inference count, failures, confidence multiplied by 1000, confirmed actions, and LED state.

Telemetry uses the Nano mbed USB CDC nonblocking sender. The application starts
without a serial monitor after a two-second startup timeout. Keep interrupts
enabled during DSP and inference; only short counter snapshots mask interrupts.

Run `sh tests/run_tests.sh` on a host with Python 3 and a C++ compiler. Tests cover
queue overflow/order/wraparound, packed FFT bounds and bins, simulated inference
stalls, filter transitions/cooldown, GPIO toggling with a fake pin, and timing
accumulation/timer wraparound. Hardware measurements are recorded separately in
`docs/validation.md`.
