#include "kws_pipeline.h"
#include "audio_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "device_control.h"
#include "telemetry.h"
#include <Arduino.h>
#include <cmsis_os2.h>
#include <rtx_os.h>
#include <atomic>

namespace {
    class Application {
    private:
        static constexpr uint32_t runFlag = 1, audioFlag = 2;
        static constexpr uint32_t eventCapacity = 8;
        alignas(8) uint8_t audioStack[8192], controlStack[6144];
        osRtxThread_t audioControlBlock, deviceControlBlock;
        osRtxMessageQueue_t queueControlBlock;
        alignas(
            8) uint8_t queueStorage[osRtxMessageQueueMemSize(eventCapacity, sizeof(KeywordEvent))];
        osRtxMutex_t metricsControlBlock;
        osThreadId_t audioThread = nullptr, controlThread = nullptr;
        osMessageQueueId_t events = nullptr;
        osMutexId_t metricsMutex = nullptr;
        std::atomic<uint32_t> generation{0}, eventDrops{0}, notifyErrors{0};
        PipelineMetrics sharedAudio;
        AudioCapture capture;
        FeatureExtractor features;
        DeviceControl device;
        Telemetry telemetry;
        AudioPipeline audioPipeline{capture, features, KeywordModel::instance()};

        uint32_t ticksForMs(uint32_t ms) {
            return (ms * osKernelGetTickFreq() + 999) / 1000;
        }
        void notifyAudio() {
            if (osThreadFlagsSet(audioThread, audioFlag) & osFlagsError)
                ++notifyErrors;
        }
        void publishAudio() {
            osMutexAcquire(metricsMutex, osWaitForever);
            sharedAudio = audioPipeline.statistics();
            osMutexRelease(metricsMutex);
        }
        void processAudio() {
            osThreadFlagsWait(runFlag, osFlagsWaitAny, osWaitForever);
            uint32_t sequence = 0;
            while (true) {
                // Flags coalesce; drain the ring completely before waiting again.
                osThreadFlagsWait(audioFlag, osFlagsWaitAny, ticksForMs(100));
                while (true) {
                    KeywordEvent event{};
                    AudioStep step = audioPipeline.process(event);
                    if (step.reset)
                        ++generation;
                    if (step.hasEvent) {
                        event.sequence = ++sequence;
                        event.generation = generation.load();
                        if (osMessageQueuePut(events, &event, 0, 0) != osOK) {
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
            osThreadFlagsWait(runFlag, osFlagsWaitAny, osWaitForever);
            KeywordEventFilter filter;
            TimingStats response, queueLatency;
            uint32_t triggers = 0, staleEvents = 0;
#ifdef KWS_RTOS_STRESS_TEST
            bool stressDone = false;
#endif
            while (true) {
                KeywordEvent event{};
                if (osMessageQueueGet(events, &event, nullptr, ticksForMs(20)) == osOK) {
#ifdef KWS_RTOS_STRESS_TEST
                    // Test-only build: block control long enough to overflow the score queue.
                    if (!stressDone) {
                        stressDone = true;
                        osDelay(ticksForMs(1500));
                    }
#endif
                    uint32_t now = micros();
                    queueLatency.record(now - event.decisionUs);
                    auto result = filter.consume(event, generation.load(), now, millis());
                    if (result.discarded)
                        ++staleEvents;
                    if (result.detection.confirmed && event.generation == generation.load()) {
                        device.applyKeywordAction();
                        ++triggers;
                        if (result.detection.hasResponse)
                            response.record(result.detection.responseUs + uint32_t(micros() - now));
                    }
                }
                PipelineMetrics snapshot;
                osMutexAcquire(metricsMutex, osWaitForever);
                snapshot = sharedAudio;
                osMutexRelease(metricsMutex);
                snapshot.confirmedKeywords = triggers;
                snapshot.responseTiming = response;
                snapshot.eventLatency = queueLatency;
                snapshot.eventDrops = eventDrops.load();
                snapshot.staleEvents = staleEvents;
                snapshot.notifyErrors = notifyErrors.load();
                snapshot.audioStackFree = osThreadGetStackSpace(audioThread);
                snapshot.controlStackFree = osThreadGetStackSpace(controlThread);
                telemetry.pump();
                telemetry.report(capture.snapshot(), snapshot, device.isOn());
            }
        }
        void cleanup() {
            capture.end(); // Stop the interrupt producer before deleting thread IDs.
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
        }

    public:
        bool begin() {
            device.begin();
            telemetry.begin();
            if (!KeywordModel::instance().begin()) {
                Serial.println("Model initialization failed");
                return false;
            }
            if (!features.begin(KeywordModel::instance().inputScale(),
                    KeywordModel::instance().inputZeroPoint())) {
                Serial.println("DSP initialization failed");
                return false;
            }
            osMutexAttr_t mutexAttrs{};
            mutexAttrs.name = "kws-metrics";
            mutexAttrs.attr_bits = osMutexPrioInherit;
            mutexAttrs.cb_mem = &metricsControlBlock;
            mutexAttrs.cb_size = sizeof(metricsControlBlock);
            metricsMutex = osMutexNew(&mutexAttrs);
            osMessageQueueAttr_t queueAttrs{};
            queueAttrs.name = "kws-scores";
            queueAttrs.cb_mem = &queueControlBlock;
            queueAttrs.cb_size = sizeof(queueControlBlock);
            queueAttrs.mq_mem = queueStorage;
            queueAttrs.mq_size = sizeof(queueStorage);
            events = osMessageQueueNew(eventCapacity, sizeof(KeywordEvent), &queueAttrs);
            if (!metricsMutex || !events) {
                cleanup();
                Serial.println("RTOS objects failed");
                return false;
            }
            osThreadAttr_t attrs{};
            attrs.name = "kws-audio";
            attrs.priority = osPriorityAboveNormal;
            attrs.cb_mem = &audioControlBlock;
            attrs.cb_size = sizeof(audioControlBlock);
            attrs.stack_mem = audioStack;
            attrs.stack_size = sizeof(audioStack);
            audioThread = osThreadNew(
                [](void *context) { static_cast<Application *>(context)->processAudio(); }, this,
                &attrs);
            attrs.name = "kws-control";
            attrs.priority = osPriorityNormal;
            attrs.cb_mem = &deviceControlBlock;
            attrs.cb_size = sizeof(deviceControlBlock);
            attrs.stack_mem = controlStack;
            attrs.stack_size = sizeof(controlStack);
            controlThread = osThreadNew(
                [](void *context) { static_cast<Application *>(context)->controlDevice(); }, this,
                &attrs);
            if (!audioThread || !controlThread) {
                cleanup();
                Serial.println("RTOS tasks failed");
                return false;
            }
            if (!capture.begin(
                    [](void *context) { static_cast<Application *>(context)->notifyAudio(); },
                    this)) {
                cleanup();
                Serial.println("PDM failed");
                return false;
            }
            Serial.println("Setup complete. RTOS audio/control tasks started.");
            osThreadFlagsSet(controlThread, runFlag);
            osThreadFlagsSet(audioThread, runFlag);
            return true;
        }
        void poll() {
            // Arduino's existing main task has no pipeline work; let application tasks run.
            osDelay(ticksForMs(1000));
        }
    };

    Application application;
} // namespace

namespace KwsPipeline {
    bool begin() {
        return application.begin();
    }
    void poll() {
        application.poll();
    }
} // namespace KwsPipeline
