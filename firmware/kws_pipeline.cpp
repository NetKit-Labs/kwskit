#include "kws_pipeline.h"

#include <array>
#include <atomic>
#include <cstdint>

#include <Arduino.h>
#include <zephyr/kernel.h>

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
        static constexpr std::uint32_t eventCapacity = 8;

        // Fixed kernel storage outlives both workers; neither thread owns these objects.
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

        // Audio is the sole writer. Loads/stores avoid retrying atomic RMW loops.
        std::atomic<std::uint32_t> generation{0}, eventDrops{0};
        PipelineMetrics sharedAudio;

        struct ControlMetrics {
            TimingStats response, queueLatency;
            std::uint32_t triggers = 0, staleEvents = 0;
            bool deviceOn = false;
        } sharedControl;

        PipelineMetrics latestTelemetry;
        bool latestDeviceOn = false;
        std::uint32_t lastStackCheck = 0;
        bool haveStackSample = false;
        bool started = false;

        AudioCapture capture;
        FeatureExtractor features;
        DeviceControl device;
        Telemetry telemetry;
        AudioPipeline audioPipeline{capture, features, KeywordModel::instance()};

        void notifyAudio() {
            // The core PDM thread publishes samples before this coalesced wakeup.
            k_sem_give(&audioReady);
        }

        void publishAudio() {
            const MutexLock lock(metricsMutex);
            if (lock)
                sharedAudio = audioPipeline.statistics();
        }

        void invalidateGeneration() {
            generation.store(
                generation.load(std::memory_order_relaxed) + 1, std::memory_order_release);
        }

        void processAudio() {
            std::uint32_t sequence = 0;

            while (true) {
                // Notifications coalesce; drain complete hops within a finite activation budget.
                k_sem_take(&audioReady, K_MSEC(100));
                constexpr unsigned maxStepsPerWake = 8;
                unsigned steps = 0;
                for (; steps < maxStepsPerWake; ++steps) {
                    KeywordEvent event{};
                    AudioStep step = audioPipeline.process(event);

                    if (step.reset)
                        invalidateGeneration();

                    if (step.hasEvent) {
                        event.sequence = ++sequence;
                        event.generation = generation.load(std::memory_order_relaxed);
                        if (k_msgq_put(&eventQueue, &event, K_NO_WAIT) != 0) {
                            eventDrops.store(eventDrops.load(std::memory_order_relaxed) + 1,
                                std::memory_order_relaxed);
                            invalidateGeneration(); // Queued evidence now has a gap.
                        }
                    }

                    if (step.processed)
                        publishAudio();
                    else
                        break;
                }
                if (steps == maxStepsPerWake) {
                    // Bound each activation even under sustained backlog, then let control run.
                    k_sem_give(&audioReady);
                    k_msleep(1);
                }
            }
        }

        void controlDevice() {
            KeywordEventFilter filter;
            TimingStats response, queueLatency;
            std::uint32_t triggers = 0, staleEvents = 0;
#ifdef KWS_RTOS_STRESS_TEST
            bool stressDone = false;
#endif

            while (true) {
                KeywordEvent event{};
                if (k_msgq_get(&eventQueue, &event, K_MSEC(20)) == 0) {
#ifdef KWS_RTOS_STRESS_TEST
                    // Test-only build: block control long enough to overflow the score queue.
                    if (!stressDone) {
                        stressDone = true;
                        k_msleep(1500);
                    }
#endif
                    std::uint32_t now = micros();
                    queueLatency.record(now - event.decisionUs);
                    auto result = filter.consume(
                        event, generation.load(std::memory_order_acquire), now, millis());
                    if (result.discarded)
                        ++staleEvents;
                    if (result.detection.confirmed) {
                        // Serialize generation validation with audio-task invalidation.
                        // GPIO was initialized before capture; this cannot allocate or wait.
                        const SchedulerLock lock;
                        if (lock &&
                            KeywordEventFilter::isCurrent(event,
                                generation.load(std::memory_order_acquire), micros(), millis())) {
                            device.applyKeywordAction();
                            ++triggers;
                            if (result.detection.hasResponse)
                                response.record(result.detection.responseUs +
                                    static_cast<std::uint32_t>(micros() - now));
                        }
                    }
                }

                // Diagnostics may miss a publication; detection never waits for them.
                const MutexLock lock(metricsMutex);
                if (lock)
                    sharedControl = {response, queueLatency, triggers, staleEvents, device.isOn()};
            }
        }

        void cleanup() {
            capture.end(); // Stop the PDM producer before aborting consumers.

            if (audioThread)
                k_thread_abort(audioThread);
            if (controlThread)
                k_thread_abort(controlThread);
            audioThread = controlThread = nullptr;
            if (rtosInitialized)
                k_msgq_purge(&eventQueue);
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

            k_mutex_init(metricsMutex);
            k_msgq_init(&eventQueue, queueStorage.data(), sizeof(KeywordEvent), eventCapacity);
            k_sem_init(&audioReady, 0, 1);
            rtosInitialized = true;
            // K_FOREVER creates dormant workers. Lower Zephyr priority numbers run first.
            // Start them only after capture and all shared state are initialized.
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

            // Serial has internal waits: keep diagnostics below both critical workers.
            k_thread_priority_set(k_current_get(), 6);
            k_thread_start(controlThread);
            k_thread_start(audioThread);

            started = true;
            return true;
        }

        void poll() {
            if (!started) {
                k_msleep(1000); // Startup failure must yield instead of busy-spinning.
                return;
            }

            std::uint32_t audioStackFree = latestTelemetry.audioStackFree;
            std::uint32_t controlStackFree = latestTelemetry.controlStackFree;
            {
                const MutexLock lock(metricsMutex);
                if (lock) {
                    latestTelemetry = sharedAudio;
                    latestTelemetry.confirmedKeywords = sharedControl.triggers;
                    latestTelemetry.responseTiming = sharedControl.response;
                    latestTelemetry.eventLatency = sharedControl.queueLatency;
                    latestTelemetry.staleEvents = sharedControl.staleEvents;
                    latestDeviceOn = sharedControl.deviceOn;
                }
            }
            latestTelemetry.eventDrops = eventDrops.load(std::memory_order_relaxed);

            // Optional stack scans and Serial calls belong only to this best-effort thread.
            const auto nowMs = millis();
            if (!haveStackSample || nowMs - lastStackCheck >= 2000) {
#if defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_STACK_INFO)
                std::size_t audioFree = 0, controlFree = 0;
                if (k_thread_stack_space_get(audioThread, &audioFree) == 0)
                    audioStackFree = audioFree;
                if (k_thread_stack_space_get(controlThread, &controlFree) == 0)
                    controlStackFree = controlFree;
#endif
                lastStackCheck = nowMs;
                haveStackSample = true;
            }
            latestTelemetry.audioStackFree = audioStackFree;
            latestTelemetry.controlStackFree = controlStackFree;
            telemetry.pump();
            telemetry.report(capture.snapshot(), latestTelemetry, latestDeviceOn);
            k_msleep(20);
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
