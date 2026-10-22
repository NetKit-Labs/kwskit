# TensorFlow Lite Micro compatibility declarations

The pinned Arduino_TensorFlowLite source revision needs three additional
Apache-2.0 TensorFlow headers at `src/tensorflow/lite/micro/`:

- `micro_error_reporter.h`
- `flatbuffer_conversions_bridge.h`
- `op_resolver_bridge.h`

These expose reporter/bridge declarations used by the existing runtime
implementations and model adapter. Setup copies these tracked headers into the
project-local dependency. Their original notices and accompanying `LICENSE`
are retained. Review them when changing the locked source revision.

Zephyr uses the core PDM driver. Setup excludes the inference library's unrelated
I2S/peripheral example sources and serial-test constructor, removes the
peripheral include from `TensorFlowLite.h`, and retains only the generic
`peripherals/utility.h` declarations needed to compile its optional timer unit.
The inference runtime and CMSIS-NN sources are retained. There are no platform
selection guards or alternative runtime implementations in this overlay.

Transformations live in `scripts/project.py` and are included in the installed
library manifest. `make build` rejects changed/missing dependency files and
changed tracked headers until `make setup` restores them. Global libraries and
the user's Arduino sketchbook are not modified. Native tests check exclusion
of example sources while preserving the inference source and timer header.
