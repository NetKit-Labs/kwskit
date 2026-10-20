#pragma once
struct osRtxThread_t {
    uint64_t data[16];
};
struct osRtxMessageQueue_t {
    uint64_t data[8];
};
struct osRtxMutex_t {
    uint64_t data[8];
};
#define osRtxMessageQueueMemSize(n, s) ((n) * ((s) + 16))
