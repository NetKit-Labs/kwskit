#pragma once

#include "cpp_standard.h"

#include <cstdint>

#ifdef ARDUINO_ARCH_ZEPHYR
#include <zephyr/kernel.h>

// Zephyr scheduler locks are recursive and leave interrupts enabled.
class SchedulerLock {
public:
    SchedulerLock() {
        k_sched_lock();
    }

    ~SchedulerLock() {
        k_sched_unlock();
    }

    explicit operator bool() const noexcept {
        return true;
    }

    SchedulerLock(const SchedulerLock &) = delete;
    SchedulerLock &operator=(const SchedulerLock &) = delete;
};
#else
#include <cmsis_os2.h>

// Task context only. Use solely around the final event check and preinitialized GPIO write.
// Interrupts remain enabled; do not wait, allocate, or perform I/O beyond that GPIO write.
class SchedulerLock {
public:
    SchedulerLock() : previousState(osKernelLock()) {}

    ~SchedulerLock() {
        if (previousState >= 0)
            osKernelRestoreLock(previousState);
    }

    explicit operator bool() const noexcept {
        return previousState >= 0;
    }

    SchedulerLock(const SchedulerLock &) = delete;
    SchedulerLock &operator=(const SchedulerLock &) = delete;

private:
    const std::int32_t previousState;
};
#endif
