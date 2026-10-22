#pragma once

#include "cpp_standard.h"

namespace KwsPipeline {
    // Initialize the model, DSP, kernel objects and capture before releasing the workers.
    bool begin();

    // Idle Arduino's main thread; the Zephyr workers run the continuous pipeline.
    void poll();
} // namespace KwsPipeline
