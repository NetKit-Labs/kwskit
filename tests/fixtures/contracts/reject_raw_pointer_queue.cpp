// Queue destinations must retain their extent through an array reference.
#include "common.h"

int main() {
    AudioQueue<8> queue;
    std::int16_t sample = 0;
    queue.pop(&sample, 1);
}
