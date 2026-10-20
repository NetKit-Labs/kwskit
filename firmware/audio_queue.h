#pragma once
#include <stdint.h>
#include <stddef.h>
#include <atomic>

// Single interrupt producer, single main-loop consumer. Never overwrite unread audio.
static_assert(ATOMIC_INT_LOCK_FREE == 2, "ISR queue requires lock-free atomics");
template <size_t Capacity> class AudioQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Power-of-two capacity required");
    int16_t samples[Capacity];
    std::atomic<uint32_t> head{0}, tail{0};

public:
    uint32_t droppedSamples = 0; // producer-owned; snapshot with interrupts disabled
    bool push(int16_t sample) {
        uint32_t h = head.load(std::memory_order_relaxed);
        if (h - tail.load(std::memory_order_acquire) == Capacity) {
            ++droppedSamples;
            return false;
        }
        samples[h & (Capacity - 1)] = sample;
        head.store(h + 1, std::memory_order_release);
        return true;
    }
    bool pop(int16_t *destination, size_t count) {
        uint32_t t = tail.load(std::memory_order_relaxed);
        if (head.load(std::memory_order_acquire) - t < count)
            return false;
        for (size_t i = 0; i < count; ++i)
            destination[i] = samples[(t + i) & (Capacity - 1)];
        tail.store(t + count, std::memory_order_release);
        return true;
    }
    uint32_t available() const {
        return head.load(std::memory_order_acquire) - tail.load(std::memory_order_acquire);
    }
};
