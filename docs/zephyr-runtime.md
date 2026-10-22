# Arduino Zephyr runtime and memory

The supported core is **Arduino Zephyr Boards 1.0.0** on the Nano 33 BLE Sense.
The core supplies Zephyr and the loader; Arduino APIs provide PDM, GPIO, timing
and USB serial, while the application uses native Zephyr kernel synchronization.
See [build](build.md) for commands and exact dependency pins.

## Loader and static linking

Arduino's core uses a prebuilt loader to host sketches. The standard package
includes the Nano's matching loader and exported-symbol/linker information.
Arduino CLI's upload recipe checks its version and installs/updates it when
necessary, then writes the packaged sketch. The original board USB bootloader
remains the recovery route. A manual double RESET may be needed to enter it.

The project selects `link_mode=static`: application code and immutable tables
stay in flash and mutable sketch state occupies the loader's reserved **128 KiB**
RAM region. An intermediate `.elf`, `.bin` or `.hex` is not a substitute for
the core's packaged `bin-zsk.bin`; use the Arduino upload recipe.
No custom loader, separate Zephyr SDK or west installation is needed.

The recorded normal image uses 206,032 bytes of its 557,056-byte sketch flash
region and 118,096 bytes of its 131,072-byte sketch RAM region. Both application
stacks, ring, DSP workspace, interpreter state and the 64 KiB arena are already
included. Its 12,976-byte remainder is not system free heap. Future builds may
change layout: consult the build output and manifest rather than assuming the
recorded footprint applies to another image.

## PDM and library configuration

The application retains `PDM.begin(1, 16000)`, `onReceive`, `available`, `read`,
`setGain(60)` and `end`. The bundled driver delivers callbacks from a core thread.
The same build properties configure both driver and application:

```text
-DPDM_NUMBER_OF_SAMPLES=256 -DPDM_THREAD_PRIORITY=3
```

Builds explicitly select a staged copy of this driver with a 1 ms backoff after
negative device reads. The [compatibility note](../build-support/pdm/README.md)
identifies the patch; successful reads retain the upstream PDM behavior. Its
source hashes appear in the build manifest, and the original core is unchanged.

The callback copies at most 512 bytes per invocation into the SPSC ring and wakes
the audio thread. Audio priority 4 and control priority 5 keep the producer ahead
of both consumers. A microphone DMA queue can lose data below the application
ring; absence of application loss counters is not proof against every driver fault.

CMSIS-DSP builds from source for the Cortex-M4F hard-float ABI. Its upstream
architecture metadata may produce a compatibility warning; compilation and live
feature extraction are the compatibility evidence. TensorFlow Lite retains its
pinned inference/CMSIS-NN source; unrelated peripheral example and serial-test
sources are excluded from staging. Runtime tensor allocation is checked before
capture. The recorded use was 12,500 bytes of the fixed 65,536-byte arena.

## Serial and diagnostic limits

Main runs telemetry at priority 6, below audio and keyword control. It requests
at most 64 bytes per pump after checking connection and `availableForWrite`.
The core still takes internal `K_FOREVER` semaphore waits and can sleep on a new
USB connection; the space check does not prove a nonblocking Serial call. These
calls never execute in either critical application worker. Main holds no metrics,
scheduler or IRQ lock while accessing Serial. Startup may wait up to two seconds for a monitor; runtime does not require
one. A pending record can be delivered on reconnect before fresh records.
The first runtime report starts a two-second interval after initialization, so
startup time is not presented as an aged pre-capture audio record.

The stock loader disables `CONFIG_INIT_STACKS` and does not export
`k_thread_stack_space_get`. `stack_free=0/0` is explicitly unavailable data. Use
`--allow-unavailable-stack-metrics` for hardware capture with this loader. The
option accepts exactly the zero/zero pair; it does not disable loss, error,
freshness, queue-capacity or progress checks. Measuring headroom needs a loader
that enables and exports the required API. Peak system heap and system-stack
usage have not been measured.

For failures before USB appears, the core documents default-UART diagnostics or
its debug mode. Changing loader features/configuration requires a separate
loader build and renewed validation; none is bundled with this project.

## Official core sources

- [Core release and installation](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/README.md)
- [Board and static-mode settings](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/boards.txt)
- [Upload and compile recipes](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/platform.txt)
- [PDM driver](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/libraries/PDM/src/PDM.cpp)

[Validation](validation.md) identifies actual builds and hardware sessions.
