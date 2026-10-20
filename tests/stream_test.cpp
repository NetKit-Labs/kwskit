#include "../firmware/audio_queue.h"
#include <assert.h>
// Simulate 60 ms inference pauses and 1 ms DSP work for 10 s at 16 kHz.
int main() {
    AudioQueue<8192> q;
    int16_t hop[256];
    int expected = 0, busy = 0, frames = 0;
    for (int ms = 0; ms < 10000; ++ms) {
        for (int i = 0; i < 16; ++i)
            assert(q.push((int16_t)(ms * 16 + i)));
        if (busy) {
            --busy;
            continue;
        }
        if (q.pop(hop, 256)) {
            for (int i = 0; i < 256; ++i)
                assert(hop[i] == (int16_t)expected++);
            ++frames;
            if (frames >= 62 && frames % 8 == 0 && q.available() < 256)
                busy = 60;
        }
    }
    assert(q.droppedSamples == 0);
    assert(expected > 150000);
}
