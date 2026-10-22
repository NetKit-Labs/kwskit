#include <cassert>
#include <cstddef>
#include <cstdint>

constexpr int K_NO_WAIT = 0;
int pdm_rx_msgq = 0, pdm_slab = 0, block = 0;
int reads = 0, errorReads = 0, sleepCalls = 0, queued = 0, freed = 0, callbacks = 0;
bool persistentFailure = false, consumeBlock = true;
void (*_onReceive)() = nullptr;

struct StopDriver {};

struct PdmFake {
    int remaining = 0;

    int available() const {
        return remaining;
    }
} PDM;

int pdm_read(void **buffer, std::uint32_t *size) {
    // Every failed read must yield before another device access is attempted.
    assert(sleepCalls == errorReads);
    if (reads == (persistentFailure ? 1000 : 6))
        throw StopDriver{};
    const auto attempt = reads++;
    if (persistentFailure || attempt < 3 || attempt == 5) {
        ++errorReads;
        return -5;
    }
    *buffer = &block;
    *size = 512;
    return 0;
}

void k_msleep(int ms) {
    assert(ms == 1 && sleepCalls < errorReads);
    ++sleepCalls;
}

int k_msgq_put(int *queue, void **buffer, int wait) {
    assert(queue == &pdm_rx_msgq && *buffer == &block && wait == K_NO_WAIT);
    PDM.remaining = 512;
    return queued++ == 0 ? 0 : -1;
}

void k_mem_slab_free(int *slab, void *buffer) {
    assert(slab == &pdm_slab && buffer == &block);
    ++freed;
}

#include "pdm_thread.inc"

void runDriver() {
    reads = errorReads = sleepCalls = queued = freed = callbacks = 0;
    try {
        pdm_thread(nullptr, nullptr, nullptr);
        assert(false);
    } catch (const StopDriver &) {
    }
    assert(sleepCalls == errorReads);
}

int main() {
    _onReceive = [] {
        ++callbacks;
        if (consumeBlock)
            PDM.remaining = 0;
    };
    runDriver();
    assert(errorReads == 4 && queued == 2 && callbacks == 1 && freed == 1);
    consumeBlock = false;
    runDriver();
    assert(callbacks == 1); // A callback making no progress cannot spin forever.
    persistentFailure = true;
    runDriver();
    assert(errorReads == 1000 && sleepCalls == 1000 && callbacks == 0 && queued == 0);
}
