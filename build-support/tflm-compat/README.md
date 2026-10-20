# TFLite Micro compatibility headers

These three Apache-2.0 TensorFlow headers were present as additional files in the
local Arduino_TensorFlowLite installation used by the validated firmware:

- `micro_error_reporter.h`
- `flatbuffer_conversions_bridge.h`
- `op_resolver_bridge.h`

They expose the legacy reporter/bridge declarations expected by the pinned
library and this application's model wrapper. The pinned upstream tree contains
related bridge implementations; its Arduino package needs these declarations at
`src/tensorflow/lite/micro/`. Setup copies these tracked files to that location
inside the project-local dependency installation. No global library is patched.

The headers retain their original TensorFlow copyright and Apache-2.0 notices.
Their declarations use the repository's four-space C++ formatting; this is a
presentation change to the tracked overlay, with no API changes. Rerun
`make setup` after an overlay update so dependency hashes include the maintained
headers. Downloaded upstream source is otherwise preserved.
The accompanying `LICENSE` is the upstream Apache License, Version 2.0. Review
this overlay whenever changing the TFLite source revision; it is specific to the
locked version, rather than a general compatibility layer for newer releases.
