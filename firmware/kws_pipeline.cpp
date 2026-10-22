#include "kws_pipeline.h"

#include <array>
#include <atomic>
#include <cstdint>

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ZEPHYR
#include <zephyr/kernel.h>
#else
#include <cmsis_os2.h>
#include <rtx_os.h>
#endif

#include "audio_capture.h"
#include "audio_pipeline.h"
#include "device_control.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "mutex_lock.h"
#include "scheduler_lock.h"
#include "telemetry.h"

namespace {
    class Application {
    private:
#ifndef ARDUINO_ARCH_ZEPHYR
        static constexpr std::uint32_t runFlag = 1, audioFlag = 2;
#endif
        static constexpr std::uint32_t eventCapacity = 8;

#ifdef ARDUINO_ARCH_ZEPHYR
        K_KERNEL_STACK_MEMBER(audioStack, 8192);
        K_KERNEL_STACK_MEMBER(controlStack, 6144);
        k_thread audioControlBlock{}, deviceControlBlock{};
        k_tid_t audioThread = nullptr, controlThread = nullptr;
        k_msgq eventQueue{};
        std::array<char, eventCapacity * sizeof(KeywordEvent)> queueStorage{};
        k_mutex metricsControlBlock{};
        k_mutex *metricsMutex = &metricsControlBlock;
        k_sem audioReady{};
        bool rtosInitialized = false;
#else
        alignas(8) std::array<std::uint8_t, 8192> audioStack{};
        alignas(8) std::array<std::uint8_t, 6144> controlStack{};
        osRtxThread_t audioControlBlock{}, deviceControlBlock{};
        osRtxMessageQueue_t queueControlBlock{};
        alignas(8) std::array<std::uint8_t,
            osRtxMessageQueueMemSize(eventCapacity, sizeof(KeywordEvent))> queueStorage{};
        osRtxMutex_t metricsControlBlock{};

        osThreadId_t audioThread = nullptr, controlThread = nullptr;
        osMessageQueueId_t events = nullptr;
        osMutexId_t metricsMutex = nullptr;

#endif

        std::atomic<std::uint32_t> generation{0}, eventDrops{0}, notifyErrors{0};
        PipelineMetrics sharedAudio;
        bool started = false;

        AudioCapture capture;
        FeatureExtractor features;
        DeviceControl device;
        Telemetry telemetry;
        AudioPipeline audioPipeline{capture, features, KeywordModel::instance()};

#ifdef ARDUINO_ARCH_ZEPHYR
        void notifyAudio() {
            // A binary semaphore coalesces callbacks and can be given from an ISR.
            k_sem_give(&audioReady);
        }
#else
        std::uint32_t ticksForMs(std::uint32_t ms) {
            return (ms * osKernelGetTickFreq() + 999) / 1000;
        }

        void notifyAudio() {
            if (osThreadFlagsSet(audioThread, audioFlag) & osFlagsError)
                ++notifyErrors;
        }

#endif

        void publishAudio() {
            const MutexLock lock(metricsMutex);
            if (lock)
                sharedAudio = audioPipeline.statistics();
        }

        void processAudio() {
#ifndef ARDUINO_ARCH_ZEPHYR
            osThreadFlagsWait(runFlag, osFlagsWaitAny, osWaitForever);
#endif
            std::uint32_t sequence = 0;

            while (true) {
                // Flags coalesce; drain the ring completely before waiting again.
#ifdef ARDUINO_ARCH_ZEPHYR
                k_sem_take(&audioReady, K_MSEC(100));
#else
                osThreadFlagsWait(audioFlag, osFlagsWaitAny, ticksForMs(100));
#endif
                while (true) {
                    KeywordEvent event{};
                    AudioStep step = audioPipeline.process(event);

                    if (step.reset)
                        ++generation;

                    if (step.hasEvent) {
                        event.sequence = ++sequence;
                        event.generation = generation.load();
#ifdef ARDUINO_ARCH_ZEPHYR
                        if (k_msgq_put(&eventQueue, &event, K_NO_WAIT) != 0) {
#else
                        if (osMessageQueuePut(events, &event, 0, 0) != osOK) {
#endif
                            ++eventDrops;
                            ++generation; // Invalidate any older queued evidence after a gap.
                        }
                    }

                    if (step.processed)
                        publishAudio();
                    else
                        break;
                }
            }
        }

        void controlDevice() {
#ifndef ARDUINO_ARCH_ZEPHYR
            osThreadFlagsWait(runFlag, osFlagsWaitAny, osWaitForever);
#endif

            KeywordEventFilter filter;
            TimingStats response, queueLatency;
            PipelineMetrics latestAudio;
            std::uint32_t lastStackCheck = 0, audioStackFree = 0, controlStackFree = 0;
            bool haveStackSample = false;
            std::uint32_t triggers = 0, staleEvents = 0;
#ifdef KWS_RTOS_STRESS_TEST
            bool stressDone = false;
#endif

            while (true) {
                KeywordEvent event{};
#ifdef ARDUINO_ARCH_ZEPHYR
                if (k_msgq_get(&eventQueue, &event, K_MSEC(20)) == 0) {
#else
                if (osMessageQueueGet(events, &event, nullptr, ticksForMs(20)) == osOK) {
#endif
#ifdef KWS_RTOS_STRESS_TEST
                    // Test-only build: block control long enough to overflow the score queue.
                    if (!stressDone) {
                        stressDone = true;
#ifdef ARDUINO_ARCH_ZEPHYR
                        k_msleep(1500);
#else
                        osDelay(ticksForMs(1500));
#endif
                    }
#endif
                    std::uint32_t now = micros();
                    queueLatency.record(now - event.decisionUs);
                    auto result = filter.consume(event, generation.load(), now, millis());
                    if (result.discarded)
                        ++staleEvents;
                    if (result.detection.confirmed) {
                        // Serialize generation validation with audio-task invalidation.
                        // GPIO was initialized before capture; this cannot allocate or wait.
                        const SchedulerLock lock;
                        if (lock &&
                            KeywordEventFilter::isCurrent(
                                event, generation.load(), micros(), millis())) {
                            device.applyKeywordAction();
                            ++triggers;
                            if (result.detection.hasResponse)
                                response.record(result.detection.responseUs +
                                    static_cast<std::uint32_t>(micros() - now));
                        }
                    }
                }

                {
                    const MutexLock lock(metricsMutex);
                    if (lock)
                        latestAudio = sharedAudio;
                }

                auto snapshot = latestAudio;
                snapshot.confirmedKeywords = triggers;
                snapshot.responseTiming = response;
                snapshot.eventLatency = queueLatency;
                snapshot.eventDrops = eventDrops.load();
                snapshot.staleEvents = staleEvents;
                snapshot.notifyErrors = notifyErrors.load();

                // RTX scans stack fill words in an SVC; keep that diagnostic work infrequent.
                const auto nowMs = millis();
                if (!haveStackSample || nowMs - lastStackCheck >= 2000) {
#ifdef ARDUINO_ARCH_ZEPHYR
#if defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_STACK_INFO)
                    std::size_t audioFree = 0, controlFree = 0;
                    if (k_thread_stack_space_get(audioThread, &audioFree) == 0)
                        audioStackFree = audioFree;
                    if (k_thread_stack_space_get(controlThread, &controlFree) == 0)
                        controlStackFree = controlFree;
#endif
#else
                    audioStackFree = osThreadGetStackSpace(audioThread);
                    controlStackFree = osThreadGetStackSpace(controlThread);
#endif
                    lastStackCheck = nowMs;
                    haveStackSample = true;
                }

                snapshot.audioStackFree = audioStackFree;
                snapshot.controlStackFree = controlStackFree;

                telemetry.pump();
                telemetry.report(capture.snapshot(), snapshot, device.isOn());
            }
        }

        void cleanup() {
            capture.end(); // Stop the interrupt producer before deleting thread IDs.

#ifdef ARDUINO_ARCH_ZEPHYR
            if (audioThread)
                k_thread_abort(audioThread);
            if (controlThread)
                k_thread_abort(controlThread);
            audioThread = controlThread = nullptr;
            if (rtosInitialized)
                k_msgq_purge(&eventQueue);
#else
            if (audioThread)
                osThreadTerminate(audioThread);
            if (controlThread)
                osThreadTerminate(controlThread);
            if (events)
                osMessageQueueDelete(events);
            if (metricsMutex)
                osMutexDelete(metricsMutex);

            audioThread = controlThread = nullptr;
            events = nullptr;
            metricsMutex = nullptr;
#endif
            started = false;
        }

    public:
        Application() = default;
        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;

        ~Application() {
            cleanup();
        }

        bool begin() {
            if (started) {
                return true;
            }

            device.begin();
            telemetry.begin();

            auto &model = KeywordModel::instance();
            if (!model.begin()) {
                Serial.println("Model initialization failed");
                return false;
            }

            if (!features.begin(model.inputScale(), model.inputZeroPoint())) {
                Serial.println("DSP initialization failed");
                return false;
            }

#ifdef ARDUINO_ARCH_ZEPHYR
            k_mutex_init(metricsMutex);
            k_msgq_init(&eventQueue, queueStorage.data(), sizeof(KeywordEvent), eventCapacity);
            k_sem_init(&audioReady, 0, 1);
            rtosInitialized = true;
            // Delay both starts until capture and all shared state are initialized.
            audioThread = k_thread_create(
                &audioControlBlock, audioStack, K_KERNEL_STACK_SIZEOF(audioStack),
                [](void *context, void *, void *) {
                    static_cast<Application *>(context)->processAudio();
                },
                this, nullptr, nullptr, 4, 0, K_FOREVER);
            controlThread = k_thread_create(
                &deviceControlBlock, controlStack, K_KERNEL_STACK_SIZEOF(controlStack),
                [](void *context, void *, void *) {
                    static_cast<Application *>(context)->controlDevice();
                },
                this, nullptr, nullptr, 5, 0, K_FOREVER);
#else
            const osMutexAttr_t mutexAttrs{
                .name = "kws-metrics",
                .attr_bits = osMutexPrioInherit,
                .cb_mem = &metricsControlBlock,
                .cb_size = sizeof(metricsControlBlock),
            };
            metricsMutex = osMutexNew(&mutexAttrs);

            const osMessageQueueAttr_t queueAttrs{
                .name = "kws-scores",
                .cb_mem = &queueControlBlock,
                .cb_size = sizeof(queueControlBlock),
                .mq_mem = queueStorage.data(),
                .mq_size = sizeof(queueStorage),
            };
            events = osMessageQueueNew(eventCapacity, sizeof(KeywordEvent), &queueAttrs);
            if (!metricsMutex || !events) {
                cleanup();
                Serial.println("RTOS objects failed");
                return false;
            }

            const osThreadAttr_t audioAttrs{
                .name = "kws-audio",
                .cb_mem = &audioControlBlock,
                .cb_size = sizeof(audioControlBlock),
                .stack_mem = audioStack.data(),
                .stack_size = sizeof(audioStack),
                .priority = osPriorityAboveNormal,
            };
            audioThread = osThreadNew(
                [](void *context) {
                    auto &application = *static_cast<Application *>(context);
                    application.processAudio();
                },
                this, &audioAttrs);

            const osThreadAttr_t controlAttrs{
                .name = "kws-control",
                .cb_mem = &deviceControlBlock,
                .cb_size = sizeof(deviceControlBlock),
                .stack_mem = controlStack.data(),
                .stack_size = sizeof(controlStack),
                .priority = osPriorityNormal,
            };
            controlThread = osThreadNew(
                [](void *context) {
                    auto &application = *static_cast<Application *>(context);
                    application.controlDevice();
                },
                this, &controlAttrs);

            if (!audioThread || !controlThread) {
                cleanup();
                Serial.println("RTOS tasks failed");
                return false;
            }

#endif

            if (!capture.begin(
                    [](void *context) {
                        auto &application = *static_cast<Application *>(context);
                        application.notifyAudio();
                    },
                    this)) {
                cleanup();
                Serial.println("PDM failed");
                return false;
            }

#ifdef ARDUINO_ARCH_ZEPHYR
            k_thread_start(controlThread);
            k_thread_start(audioThread);
#else
            if ((osThreadFlagsSet(controlThread, runFlag) & osFlagsError) ||
                (osThreadFlagsSet(audioThread, runFlag) & osFlagsError)) {
                cleanup();
                Serial.println("RTOS start flags failed");
                return false;
            }

#endif

            started = true;
            return true;
        }

        void poll() {
            // Arduino's existing main task has no pipeline work; let application tasks run.
#ifdef ARDUINO_ARCH_ZEPHYR
            k_msleep(1000);
#else
            osDelay(ticksForMs(1000));
#endif
        }
    };

    Application &application() {
        // Construct after board globals; task/driver owners are destroyed before them.
        static Application instance;
        return instance;
    }
} // namespace

namespace KwsPipeline {
    bool begin() {
        return application().begin();
    }

    void poll() {
        application().poll();
    }
} // namespace KwsPipeline
