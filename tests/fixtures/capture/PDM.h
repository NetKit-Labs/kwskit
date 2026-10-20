#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>

struct PdmFake {
    void (*callback)() = nullptr;
    bool startOk = true;
    bool running = false;
    int stops = 0;
    size_t bufferSize = 0;
    std::deque<int16_t> pending;
    void onReceive(void (*receiver)()) {
        callback = receiver;
    }
    void setBufferSize(size_t size) {
        bufferSize = size;
    }
    bool begin(int channels, int rate);
    void end();
    void setGain(int gain);
    int available();
    int read(void *destination, int bytes);
};
extern PdmFake PDM;
