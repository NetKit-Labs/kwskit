#pragma once

#include "cpp_standard.h"

#include <cstdint>

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ZEPHYR
#include <zephyr/irq.h>
#endif

// Preserve the caller's interrupt mask, including an existing critical section.
class InterruptLock {
public:
#ifdef ARDUINO_ARCH_ZEPHYR
    InterruptLock() noexcept : previousMask(irq_lock()) {}

    ~InterruptLock() {
        irq_unlock(previousMask);
    }
#else
    InterruptLock() noexcept : previousMask(__get_PRIMASK()) {
        noInterrupts();
    }

    ~InterruptLock() {
        __set_PRIMASK(previousMask);
    }

#endif

    InterruptLock(const InterruptLock &) = delete;
    InterruptLock &operator=(const InterruptLock &) = delete;

private:
    const std::uint32_t previousMask;
};
