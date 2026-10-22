#pragma once

#include "cpp_standard.h"

#include <cstdint>

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
