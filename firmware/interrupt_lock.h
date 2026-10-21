#pragma once

#include "cpp_standard.h"

#include <cstdint>

#include <Arduino.h>

// Preserve the caller's interrupt mask, including an existing critical section.
class InterruptLock {
public:
    InterruptLock() noexcept : previousMask(__get_PRIMASK()) {
        noInterrupts();
    }

    ~InterruptLock() {
        __set_PRIMASK(previousMask);
    }

    InterruptLock(const InterruptLock &) = delete;
    InterruptLock &operator=(const InterruptLock &) = delete;

private:
    const std::uint32_t previousMask;
};
