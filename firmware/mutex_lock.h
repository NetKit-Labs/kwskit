#pragma once

#include "cpp_standard.h"

#ifdef ARDUINO_ARCH_ZEPHYR
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
#else
#include <cmsis_os2.h>

// Optional diagnostic snapshots never wait for another task. Release only if acquired.
class MutexLock {
public:
    explicit MutexLock(osMutexId_t mutex) : mutex(mutex), owned(osMutexAcquire(mutex, 0) == osOK) {}

    ~MutexLock() {
        if (owned)
            osMutexRelease(mutex);
    }

    explicit operator bool() const noexcept {
        return owned;
    }

    MutexLock(const MutexLock &) = delete;
    MutexLock &operator=(const MutexLock &) = delete;

private:
    const osMutexId_t mutex;
    const bool owned;
};
#endif
