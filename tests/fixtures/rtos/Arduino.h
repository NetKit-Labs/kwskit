#pragma once
#include <stdint.h>
uint32_t micros();
uint32_t millis();
struct SerialFake {
    void println(const char *) {}
};
extern SerialFake Serial;
