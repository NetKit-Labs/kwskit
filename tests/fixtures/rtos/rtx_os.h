#pragma once

#include <cstdint>

struct osRtxThread_t {
    std::uint64_t data[16];
};

struct osRtxMessageQueue_t {
    std::uint64_t data[8];
};

struct osRtxMutex_t {
    std::uint64_t data[8];
};

#define osRtxMessageQueueMemSize(n, s) ((n) * ((s) + 16))
