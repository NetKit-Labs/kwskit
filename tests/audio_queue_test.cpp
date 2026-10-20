#include "../firmware/audio_queue.h"
#include <assert.h>
int main() {
  AudioQueue<8> q;
  int16_t out[8];
  assert(!q.pop(out, 1));
  for (int i=0; i<8; ++i) assert(q.push(i));
  assert(!q.push(99) && q.droppedSamples == 1);
  assert(q.pop(out, 4));
  for (int i=0; i<4; ++i) assert(out[i] == i);
  for (int i=8; i<12; ++i) assert(q.push(i));
  assert(q.pop(out, 8));
  for (int i=0; i<8; ++i) assert(out[i] == i+4);
  for (int n=0; n<10000; ++n) { assert(q.push(n)); assert(q.pop(out,1)); assert(out[0] == n); }
}
