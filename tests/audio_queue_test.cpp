#include "../firmware/audio_queue.h"
#include <cstdint>
#include <cassert>
int main() {
    AudioQueue<8> q;
    std::array<std::int16_t, 8> out{};
    assert(!q.pop(out, 1));
    for (int i = 0; i < 8; ++i)
        assert(q.push(i));
    assert(!q.push(99) && q.droppedSamples() == 1);
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
    assert(q.available() == 0 && q.droppedSamples() == 1);
    assert(q.push(10) && q.push(20));
    q.discardPending();
    assert(q.available() == 0 && q.droppedSamples() == 1);
    std::array<std::int16_t, 2> pair{};
    assert(!q.pop(pair));
    assert(q.push(30) && q.push(40) && q.pop(pair));
    assert(pair[0] == 30 && pair[1] == 40);
    assert(q.push(50) && q.push(60) && q.push(70));
    // A requested count larger than the referenced destination cannot consume audio.
    assert(!q.pop(pair, 3) && q.available() == 3);
    assert(pair[0] == 30 && pair[1] == 40);
    assert(q.pop(pair) && pair[0] == 50 && pair[1] == 60);
    assert(q.pop(pair, 1) && pair[0] == 70 && q.available() == 0);
}
