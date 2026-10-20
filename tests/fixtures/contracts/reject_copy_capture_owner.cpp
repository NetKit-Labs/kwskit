// Compiler contract probe: copy capture owner.
#include "common.h"

int main() {
    AudioCapture capture;
    auto copy = capture;
    (void)copy;
}
