#pragma once
#include "cpp_standard.h"
#include <cmsis_os2.h>
#include <cstdint>

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
