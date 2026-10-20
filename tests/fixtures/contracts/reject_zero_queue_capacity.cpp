// Compiler contract probe: zero queue capacity.
#include "common.h"

int main() {
    AudioQueue<0> queue;
    (void)queue;
}
