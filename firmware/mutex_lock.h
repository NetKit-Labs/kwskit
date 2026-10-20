#pragma once
#include <cmsis_os2.h>

// Use with an initialized mutex in task context. Scope exit releases the lock.
class MutexLock {
public:
    explicit MutexLock(osMutexId_t mutex) : mutex(mutex) {
        osMutexAcquire(mutex, osWaitForever);
    }
    ~MutexLock() {
        osMutexRelease(mutex);
    }
    MutexLock(const MutexLock &) = delete;
    MutexLock &operator=(const MutexLock &) = delete;

private:
    const osMutexId_t mutex;
};
