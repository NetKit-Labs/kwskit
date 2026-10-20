#pragma once
#include <cstdint>
using osThreadId_t = void *;
using osMessageQueueId_t = void *;
using osMutexId_t = void *;
using osStatus_t = int;
constexpr int osOK = 0, osPriorityAboveNormal = 32, osPriorityNormal = 24;
constexpr std::uint32_t osWaitForever = 0xffffffffu, osFlagsError = 0x80000000u, osFlagsWaitAny = 0,
                        osMutexPrioInherit = 2;
struct osThreadAttr_t {
    const char *name;
    std::uint32_t attr_bits;
    void *cb_mem;
    std::uint32_t cb_size;
    void *stack_mem;
    std::uint32_t stack_size;
    int priority;
};
struct osMessageQueueAttr_t {
    const char *name;
    std::uint32_t attr_bits;
    void *cb_mem;
    std::uint32_t cb_size;
    void *mq_mem;
    std::uint32_t mq_size;
};
struct osMutexAttr_t {
    const char *name;
    std::uint32_t attr_bits;
    void *cb_mem;
    std::uint32_t cb_size;
};
osMutexId_t osMutexNew(const osMutexAttr_t *);
int osMutexDelete(osMutexId_t);
int osMutexAcquire(osMutexId_t, std::uint32_t);
int osMutexRelease(osMutexId_t);
osThreadId_t osThreadNew(void (*)(void *), void *, const osThreadAttr_t *);
int osThreadTerminate(osThreadId_t);
std::uint32_t osThreadGetStackSpace(osThreadId_t);
std::uint32_t osThreadFlagsSet(osThreadId_t, std::uint32_t);
std::uint32_t osThreadFlagsWait(std::uint32_t, std::uint32_t, std::uint32_t);
std::uint32_t osKernelGetTickFreq();
int osDelay(std::uint32_t);
osMessageQueueId_t osMessageQueueNew(std::uint32_t, std::uint32_t, const osMessageQueueAttr_t *);
int osMessageQueueDelete(osMessageQueueId_t);
int osMessageQueuePut(osMessageQueueId_t, const void *, std::uint8_t, std::uint32_t);
int osMessageQueueGet(osMessageQueueId_t, void *, std::uint8_t *, std::uint32_t);
