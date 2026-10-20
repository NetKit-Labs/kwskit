#pragma once
#include "cpp_standard.h"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>
#include <limits>

// Single interrupt producer, single audio-task consumer. Never overwrite unread audio.
static_assert(ATOMIC_INT_LOCK_FREE == 2, "ISR queue requires lock-free atomics");
template <std::size_t Capacity> class AudioQueue {
    static_assert(
        Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Nonzero power-of-two capacity required");
    static_assert(
        Capacity <= std::numeric_limits<std::uint32_t>::max(), "Capacity must fit ring indices");
    std::array<std::int16_t, Capacity> samples{};
    std::uint32_t droppedCount = 0;
    std::atomic<std::uint32_t> head{0}, tail{0};

public:
    // Producer-owned counter; the audio task reads it with interrupts disabled.
    std::uint32_t droppedSamples() const {
        return droppedCount;
    }
    bool push(std::int16_t sample) {
        std::uint32_t h = head.load(std::memory_order_relaxed);
        if (h - tail.load(std::memory_order_acquire) == Capacity) {
            ++droppedCount;
            return false;
        }
        samples[h & (Capacity - 1)] = sample;
        head.store(h + 1, std::memory_order_release);
        return true;
    }
    template <std::size_t Count>
    bool pop(std::array<std::int16_t, Count> &destination, std::size_t count = Count) {
        if (count > destination.size())
            return false;
        std::uint32_t t = tail.load(std::memory_order_relaxed);
        if (head.load(std::memory_order_acquire) - t < count)
            return false;
        for (std::size_t i = 0; i < count; ++i)
            destination[i] = samples[(t + i) & (Capacity - 1)];
        tail.store(t + count, std::memory_order_release);
        return true;
    }
    void discardPending() {
        // Only the consumer advances tail. Samples published afterward remain available.
        tail.store(head.load(std::memory_order_acquire), std::memory_order_release);
    }
    std::uint32_t available() const {
        return head.load(std::memory_order_acquire) - tail.load(std::memory_order_acquire);
    }
};
