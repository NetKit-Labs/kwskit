#pragma once

#include "cpp_standard.h"

#include <zephyr/kernel.h>

class MutexLock {
public:
    explicit MutexLock(k_mutex *mutex) : mutex(mutex), owned(k_mutex_lock(mutex, K_NO_WAIT) == 0) {}

    ~MutexLock() {
        if (owned)
            k_mutex_unlock(mutex);
    }

    explicit operator bool() const noexcept {
        return owned;
    }

    MutexLock(const MutexLock &) = delete;
    MutexLock &operator=(const MutexLock &) = delete;

private:
    k_mutex *const mutex;
    const bool owned;
};
