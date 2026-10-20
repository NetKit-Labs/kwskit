#include "kws_pipeline.h"
#include "mutex_lock.h"
#include "scheduler_lock.h"
#include <cstdint>
#include "audio_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "device_control.h"
#include "telemetry.h"
#include <cmsis_os2.h>
#include <Arduino.h>
#include <cassert>
#include <cstring>
#include <vector>
SerialFake Serial;
std::uint32_t clockUs = 1000000, clockMs = 1000;
std::uint32_t micros() {
    return clockUs;
}
std::uint32_t millis() {
    return clockMs;
}
int failure = 0, mutexes = 0, queues = 0, threads = 0, threadCalls = 0, lastDelay = 0,
    flagCalls = 0;
AudioCapture::Notification notifier = nullptr;
void *notificationContext = nullptr;
std::array<osThreadAttr_t, 2> attrs{};
struct Task {
    void (*entry)(void *) = nullptr;
    void *context = nullptr;
};
std::array<Task, 2> tasks{};
struct StopTask {};
std::vector<KeywordEvent> scores;
std::size_t nextScore = 0;
int actions = 0, stackQueries = 0, schedulerState = 0;
bool mutexBusy = false, schedulerFailure = false, delayAction = false;
bool invalidateAction = false, runningAudio = false;
int audioWaits = 0, audioSteps = 0;
osMutexId_t osMutexNew(const osMutexAttr_t *a) {
    assert(a->cb_mem && a->cb_size && a->attr_bits == osMutexPrioInherit);
    if (failure == 1) {
        return nullptr;
    }
    ++mutexes;
    return reinterpret_cast<void *>(std::uintptr_t{1});
}
int osMutexDelete(osMutexId_t) {
    --mutexes;
    return 0;
}
int heldLocks = 0;
int osMutexAcquire(osMutexId_t mutex, std::uint32_t wait) {
    assert(mutex && wait == 0);
    if (mutexBusy)
        return -1;
    ++heldLocks;
    return 0;
}
int osMutexRelease(osMutexId_t) {
    assert(heldLocks > 0);
    --heldLocks;
    return 0;
}
osMessageQueueId_t osMessageQueueNew(
    std::uint32_t n, std::uint32_t size, const osMessageQueueAttr_t *a) {
    assert(n == 8 && size == sizeof(KeywordEvent));
    assert(a->attr_bits == 0);
    assert(a->cb_mem && a->mq_mem && a->mq_size >= n * size);
    if (failure == 2) {
        return nullptr;
    }
    ++queues;
    return reinterpret_cast<void *>(std::uintptr_t{2});
}
int osMessageQueueDelete(osMessageQueueId_t) {
    --queues;
    return 0;
}
int osMessageQueuePut(osMessageQueueId_t, const void *, std::uint8_t, std::uint32_t wait) {
    assert(wait == 0);
    return 0;
}
int osMessageQueueGet(osMessageQueueId_t, void *destination, std::uint8_t *, std::uint32_t) {
    if (nextScore == scores.size())
        throw StopTask{};
    const auto &event = scores[nextScore++];
    clockUs = event.decisionUs + 8000;
    clockMs = event.decisionMs + 8;
    *static_cast<KeywordEvent *>(destination) = event;
    return osOK;
}
osThreadId_t osThreadNew(void (*entry)(void *), void *context, const osThreadAttr_t *a) {
    assert(context);
    assert(a->attr_bits == 0 && a->tz_module == 0 && a->reserved == 0);
    assert(entry && a->stack_mem && a->cb_mem && (std::uintptr_t(a->stack_mem) % 8) == 0);
    int call = threadCalls++;
    tasks[call % 2] = {entry, context};
    attrs[call % 2] = *a;
    if ((failure == 3 && call == 0) || (failure == 4 && call == 1))
        return nullptr;
    ++threads;
    return reinterpret_cast<void *>(std::uintptr_t(call + 3));
}
int osThreadTerminate(osThreadId_t) {
    --threads;
    return 0;
}
std::uint32_t osThreadGetStackSpace(osThreadId_t) {
    ++stackQueries;
    return 4096;
}
std::uint32_t osThreadFlagsSet(osThreadId_t id, std::uint32_t f) {
    assert(id);
    ++flagCalls;
    if ((failure == 8 && id == reinterpret_cast<void *>(std::uintptr_t{4})) ||
        (failure == 9 && id == reinterpret_cast<void *>(std::uintptr_t{3})))
        return osFlagsError;
    return f;
}
std::uint32_t osThreadFlagsWait(std::uint32_t f, std::uint32_t, std::uint32_t) {
    if (runningAudio && f == 2 && ++audioWaits > 1)
        throw StopTask{};
    return f;
}
std::uint32_t osKernelGetTickFreq() {
    return 1000;
}
int osDelay(std::uint32_t t) {
    lastDelay = t;
    return 0;
}
std::int32_t osKernelLock() {
    if (invalidateAction) {
        // Simulate audio preempting control immediately before scheduler locking.
        invalidateAction = false;
        runningAudio = true;
        audioWaits = audioSteps = 0;
        try {
            tasks[0].entry(tasks[0].context);
        } catch (const StopTask &) {
        }
        runningAudio = false;
    }
    if (delayAction) {
        clockUs += 300000;
        clockMs += 300;
    }
    if (schedulerFailure)
        return -1;
    const auto previous = schedulerState;
    schedulerState = 1;
    return previous;
}
std::int32_t osKernelRestoreLock(std::int32_t previous) {
    assert(schedulerState == 1 && previous >= 0);
    schedulerState = previous;
    return previous;
}

bool AudioCapture::begin(Notification callback, void *context) {
    notifier = callback;
    ::notificationContext = context;
    return failure != 5;
}
void AudioCapture::end() {
    notifier = nullptr;
}
AudioCapture::Snapshot AudioCapture::snapshot() const {
    return {};
}

bool KeywordModel::begin() {
    (void)runtime;
    return failure != 6;
}
float KeywordModel::inputScale() const {
    return 0.5f;
}
std::int32_t KeywordModel::inputZeroPoint() const {
    return 0;
}

bool FeatureExtractor::begin(float, std::int32_t) {
    return failure != 7;
}

void DeviceControl::begin() {
    deviceOn = false;
}
void DeviceControl::applyKeywordAction() {
    assert(schedulerState == 1);
    ++actions;
}
bool DeviceControl::isOn() const {
    return deviceOn;
}

void Telemetry::begin() {
    (void)buffer;
    length = offset = lastReport = 0;
}
void Telemetry::pump() {}
void Telemetry::report(const AudioCapture::Snapshot &, const PipelineMetrics &, bool) {}

AudioStep AudioPipeline::process(KeywordEvent &) {
    (void)capture;
    (void)extractor;
    (void)model;
    (void)seenDrops;
    (void)audioHop;
    return {false, false, audioSteps++ == 0};
}
struct KeywordModel::Runtime {};
KeywordModel &KeywordModel::instance() {
    static Runtime runtime;
    static KeywordModel model(runtime);
    return model;
}
int main() {
    const auto earlyReturn = [] {
        const MutexLock lock(reinterpret_cast<void *>(std::uintptr_t{1}));
        assert(heldLocks == 1);
        return;
    };
    earlyReturn();
    assert(heldLocks == 0);
    mutexBusy = true;
    {
        const MutexLock lock(reinterpret_cast<void *>(std::uintptr_t{1}));
        assert(!lock && heldLocks == 0);
    }
    mutexBusy = false;
    {
        const SchedulerLock outer;
        assert(outer && schedulerState == 1);
        {
            const SchedulerLock nested;
            assert(nested);
        }
        assert(schedulerState == 1);
    }
    assert(schedulerState == 0);
    schedulerFailure = true;
    {
        const SchedulerLock lock;
        assert(!lock);
    }
    assert(schedulerState == 0);
    schedulerFailure = false;
    for (int f = 1; f <= 9; ++f) {
        failure = f;
        threadCalls = 0;
        assert(!KwsPipeline::begin());
        assert(threads == 0 && queues == 0 && mutexes == 0 && !notifier);
    }
    failure = 0;
    threadCalls = 0;
    assert(KwsPipeline::begin());
    assert(threads == 2 && queues == 1 && mutexes == 1 && notifier);
    assert(attrs[0].priority > attrs[1].priority);
    assert(attrs[0].stack_size == 8192 && attrs[1].stack_size == 6144);
    assert(attrs[0].stack_mem != attrs[1].stack_mem);
    assert(std::strcmp(attrs[0].name, "kws-audio") == 0 &&
        std::strcmp(attrs[1].name, "kws-control") == 0);
    assert(KwsPipeline::begin()); // Repeated startup cannot allocate duplicate resources.
    assert(threads == 2 && queues == 1 && mutexes == 1);
    flagCalls = 0;
    notifier(notificationContext);
    assert(flagCalls == 1);
    KwsPipeline::poll();
    assert(lastDelay == 1000);
    const auto runControl = [] {
        scores.clear();
        for (std::uint32_t i = 0; i < 30; ++i) {
            const auto ms = 1000 + i * 128;
            scores.push_back({i < 3 ? 1.0f : 0.0f, ms * 1000 - 60000, ms * 1000, ms, i, 0});
        }
        nextScore = 0;
        actions = stackQueries = 0;
        try {
            tasks[1].entry(tasks[1].context);
        } catch (const StopTask &) {
        }
        assert(heldLocks == 0 && schedulerState == 0);
        assert(stackQueries == 4); // First sample, then once per two seconds; not every loop.
    };
    runControl();
    assert(actions == 1);
    delayAction = true;
    runControl();
    assert(actions == 0); // A fresh dequeue can become stale before the GPIO action.
    delayAction = false;
    schedulerFailure = true;
    runControl();
    assert(actions == 0);
    schedulerFailure = false;
    mutexBusy = true;
    runControl();
    assert(actions == 1); // Optional metrics contention does not prevent device operation.
    mutexBusy = false;
    invalidateAction = true;
    runControl();
    assert(actions == 0); // Generation changed between filtering and the final action check.
}
