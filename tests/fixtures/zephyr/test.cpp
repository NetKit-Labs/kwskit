#include <array>
#include <cassert>
#include <vector>

#include <Arduino.h>
#include <zephyr/kernel.h>

#include "audio_capture.h"
#include "audio_pipeline.h"
#include "device_control.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "kws_pipeline.h"
#include "mutex_lock.h"
#include "scheduler_lock.h"
#include "telemetry.h"

SerialFake Serial;
std::uint32_t nowUs = 1000000;

std::uint32_t micros() {
    return nowUs;
}

std::uint32_t millis() {
    return nowUs / 1000;
}

bool captureFails = true, captureRunning = false, mutexBusy = false;
int threadCount = 0, creates = 0, starts = 0, schedulerDepth = 0, actions = 0, lastDelay = 0;
std::array<k_tid_t, 2> tasks{};
AudioCapture::Notification notifier;
void *notifyContext;
k_sem *notification;
std::vector<KeywordEvent> scores;
std::size_t nextScore;

struct StopTask {};

void k_mutex_init(k_mutex *m) {
    m->held = false;
}

int k_mutex_lock(k_mutex *m, int wait) {
    assert(wait == K_NO_WAIT);
    if (mutexBusy)
        return -1;
    assert(!m->held);
    m->held = true;
    return 0;
}

void k_mutex_unlock(k_mutex *m) {
    assert(m->held);
    m->held = false;
}

void k_sem_init(k_sem *s, unsigned count, unsigned limit) {
    assert(count == 0 && limit == 1);
    *s = {count, limit};
    notification = s;
}

void k_sem_give(k_sem *s) {
    if (s->count < s->limit)
        ++s->count;
}

int k_sem_take(k_sem *s, int wait) {
    assert(wait == 100);
    if (s->count)
        --s->count;
    throw StopTask{};
}

void k_msgq_init(k_msgq *q, char *storage, std::size_t size, unsigned capacity) {
    assert(storage && size == sizeof(KeywordEvent) && capacity == 8);
    q->used = 0;
}

int k_msgq_put(k_msgq *, const void *, int wait) {
    assert(wait == 0);
    return 0;
}

int k_msgq_get(k_msgq *, void *out, int wait) {
    assert(wait == 20);
    if (nextScore == scores.size())
        throw StopTask{};
    const auto &event = scores[nextScore++];
    nowUs = event.decisionUs + 8000;
    *static_cast<KeywordEvent *>(out) = event;
    return 0;
}

void k_msgq_purge(k_msgq *q) {
    q->used = 0;
}

k_tid_t k_thread_create(k_thread *t, char *stack, std::size_t size,
    void (*entry)(void *, void *, void *), void *context, void *p2, void *p3, int priority,
    unsigned options, int delay) {
    assert(stack && std::uintptr_t(stack) % 8 == 0);
    assert(size == (creates % 2 == 0 ? 8192 : 6144));
    assert(entry && context && !p2 && !p3 && options == 0 && delay == K_FOREVER);
    *t = {entry, context, priority, false};
    tasks[creates++ % 2] = t;
    ++threadCount;
    return t;
}

void k_thread_start(k_tid_t t) {
    assert(captureRunning && !t->started);
    t->started = true;
    ++starts;
}

void k_thread_abort(k_tid_t) {
    --threadCount;
}

void k_sched_lock() {
    ++schedulerDepth;
}

void k_sched_unlock() {
    assert(schedulerDepth > 0);
    --schedulerDepth;
}

void k_msleep(int ms) {
    lastDelay = ms;
}

bool AudioCapture::begin(Notification notify, void *context) {
    assert(starts == 0); // Neither consumer can run during capture initialization.
    notifier = notify;
    notifyContext = context;
    captureRunning = !captureFails;
    return captureRunning;
}

void AudioCapture::end() {
    captureRunning = false;
    notifier = nullptr;
}

AudioCapture::Snapshot AudioCapture::snapshot() const {
    return {};
}

struct KeywordModel::Runtime {};

KeywordModel &KeywordModel::instance() {
    static Runtime storage;
    static KeywordModel model(storage);
    return model;
}

bool KeywordModel::begin() {
    (void)runtime;
    return true;
}

float KeywordModel::inputScale() const {
    return 0.5f;
}

std::int32_t KeywordModel::inputZeroPoint() const {
    return 0;
}

bool FeatureExtractor::begin(float, std::int32_t) {
    return true;
}

void DeviceControl::begin() {
    deviceOn = false;
}

void DeviceControl::applyKeywordAction() {
    assert(schedulerDepth == 1);
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

void Telemetry::report(const AudioCapture::Snapshot &, const PipelineMetrics &m, bool) {
    // Stock loader has no stack-watermark support; never fabricate headroom.
    assert(m.audioStackFree == 0 && m.controlStackFree == 0);
}

AudioStep AudioPipeline::process(KeywordEvent &) {
    (void)capture;
    (void)extractor;
    (void)model;
    (void)seenDrops;
    (void)audioHop;
    return {};
}

int main() {
    {
        const SchedulerLock outer;
        {
            const SchedulerLock inner;
            assert(inner);
        }
        assert(outer && schedulerDepth == 1);
    }
    assert(schedulerDepth == 0);
    assert(!KwsPipeline::begin());
    assert(threadCount == 0 && starts == 0 && !notifier);
    captureFails = false;
    assert(KwsPipeline::begin());
    assert(threadCount == 2 && starts == 2 && tasks[0]->priority < tasks[1]->priority);
    assert(KwsPipeline::begin() && threadCount == 2);
    notifier(notifyContext);
    notifier(notifyContext);
    assert(notification->count == 1); // Bursts coalesce without overflowing.
    KwsPipeline::poll();
    assert(lastDelay == 1000);
    for (std::uint32_t i = 0; i < 30; ++i) {
        const auto ms = 1000 + i * 128;
        scores.push_back({i < 3 ? 1.0f : 0.0f, ms * 1000 - 60000, ms * 1000, ms, i, 0});
    }
    mutexBusy = true; // Diagnostic contention must not inhibit actions.
    try {
        tasks[1]->entry(tasks[1]->context, nullptr, nullptr);
    } catch (const StopTask &) {
    }
    assert(actions == 1 && schedulerDepth == 0);
}
