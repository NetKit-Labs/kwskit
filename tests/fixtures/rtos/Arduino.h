#pragma once
#include <cstdint>
std::uint32_t micros();
std::uint32_t millis();
struct SerialFake {
    void println(const char *) {}
};
extern SerialFake Serial;
