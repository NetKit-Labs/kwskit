#include "../firmware/audio_queue.h"
#include <assert.h>
int main() {
    AudioQueue<8> q;
    int16_t out[8];
    assert(!q.pop(out, 1));
    for (int i = 0; i < 8; ++i)
        assert(q.push(i));
    assert(!q.push(99) && q.droppedSamples == 1);
    assert(q.pop(out, 4));
    for (int i = 0; i < 4; ++i)
        assert(out[i] == i);
    for (int i = 8; i < 12; ++i)
        assert(q.push(i));
    assert(q.pop(out, 8));
    for (int i = 0; i < 8; ++i)
        assert(out[i] == i + 4);
    for (int n = 0; n < 10000; ++n) {
        assert(q.push(n));
        assert(q.pop(out, 1));
        assert(out[0] == n);
    }
    // An incomplete hop must leave both unread samples and the output untouched.
    assert(q.push(123) && q.push(456));
    out[0] = out[1] = out[2] = -1;
    assert(!q.pop(out, 3) && q.available() == 2);
    assert(out[0] == -1 && out[1] == -1 && out[2] == -1);
    assert(q.push(789) && q.pop(out, 3));
    assert(out[0] == 123 && out[1] == 456 && out[2] == 789);
    assert(q.available() == 0 && q.droppedSamples == 1);
}
