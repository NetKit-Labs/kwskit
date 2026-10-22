#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

std::uint32_t millis();
std::uint32_t micros();
void delay(unsigned long ms);

struct SerialFake {
    bool connected = false;
    int space = 0;
    std::size_t writes = 0;
    std::string output;

    void begin(unsigned long rate);

    explicit operator bool() const {
        return connected;
    }

    int availableForWrite() const {
        return space;
    }

    std::size_t write(const std::uint8_t *data, std::size_t count);
};

extern SerialFake Serial;
