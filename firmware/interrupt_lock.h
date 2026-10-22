#pragma once

#include "cpp_standard.h"

#include <cstdint>

#include <Arduino.h>
#include <zephyr/irq.h>

// Preserve the caller's interrupt mask, including an existing critical section.
class InterruptLock {
public:
    InterruptLock() noexcept : previousMask(irq_lock()) {}

    ~InterruptLock() {
        irq_unlock(previousMask);
    }

    InterruptLock(const InterruptLock &) = delete;
    InterruptLock &operator=(const InterruptLock &) = delete;

private:
    const std::uint32_t previousMask;
};
