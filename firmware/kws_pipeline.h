#pragma once

#include "cpp_standard.h"

namespace KwsPipeline {
    // Initialize the model, DSP, kernel objects and capture before releasing the workers.
    bool begin();

    // Run best-effort diagnostics at priority 6; workers own audio and keyword control.
    void poll();
} // namespace KwsPipeline
