#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include <PDM.h>

#include "audio_capture.h"

PdmFake PDM;
std::uint32_t clockUs = 1000000;
int interruptMask = 0, notifications = 0;

std::uint32_t micros() {
    return clockUs;
}

void noInterrupts() {
    interruptMask = 1;
}

void interrupts() {
    interruptMask = 0;
}

std::uint32_t __get_PRIMASK() {
    return interruptMask;
}

void __set_PRIMASK(std::uint32_t mask) {
    interruptMask = static_cast<int>(mask);
}

bool PdmFake::begin(int channels, int rate) {
    assert(channels == 1 && rate == 16000);
    running = startOk;
    return running;
}

void PdmFake::end() {
    running = false;
    ++stops;
}

void PdmFake::setGain(int gain) {
    assert(gain == 60);
}

int PdmFake::available() {
    return static_cast<int>(pending.size() * sizeof(std::int16_t));
}

int PdmFake::read(void *destination, int bytes) {
    assert(bytes > 0 && static_cast<std::size_t>(bytes) <= bufferSize && bytes % 2 == 0);
    ++readCalls;
    bytes = std::min(bytes, readLimit);
    auto *samples = static_cast<std::int16_t *>(destination);
    for (int i = 0; i < bytes / 2; ++i) {
        samples[i] = pending.front();
        pending.pop_front();
    }
    return bytes;
}

int main() {
    AudioCapture capture, other;
    const auto notify = [](void *context) { ++*static_cast<int *>(context); };
    PDM.startOk = false;
    assert(!capture.begin(notify, &notifications));
    PDM.startOk = true;
    assert(capture.begin(notify, &notifications));
    assert(!other.begin());
    assert(PDM.running && PDM.bufferSize == PDM_BUFFER_SIZE * sizeof(std::int16_t));
    for (int i = 0; i < 8192 + 168; ++i) {
        PDM.pending.push_back(static_cast<std::int16_t>(i));
    }
    // A driver reporting more than a block cannot make one callback drain unbounded work.
    PDM.callback();
    assert(notifications == 1 && PDM.pending.size() == 8360 - HOP_SIZE && PDM.readCalls == 1);
    while (!PDM.pending.empty())
        PDM.callback();
    assert(notifications == 33);
    const auto stats = capture.snapshot();
    assert(stats.receivedSamples == 8360 && stats.droppedSamples == 168);
    assert(stats.missedBuffers == 1 && stats.highWater == 8192);
    assert(interruptMask == 0);
    other.end();
    assert(PDM.running && PDM.stops == 0); // A non-owner cannot stop this receiver.
    std::array<std::int16_t, HOP_SIZE> hop{};
    std::uint32_t time;
    assert(capture.readHop(hop, time));
    for (int i = 0; i < HOP_SIZE; ++i) {
        assert(hop[i] == i); // Overflow did not overwrite unread samples.
    }
    assert(
        time == clockUs - (static_cast<std::uint64_t>(8192 - HOP_SIZE) * 1000000ULL) / SAMPLE_RATE);
    noInterrupts();
    capture.snapshot();
    assert(interruptMask == 1); // Nested snapshots must preserve a disabled caller.
    assert(capture.readHop(hop, time) && interruptMask == 1);
    interrupts();
    capture.discardPending();
    assert(capture.available() == 0 && !capture.readHop(hop, time));
    PDM.pending.push_back(123);
    clockUs += 16000;
    PDM.callback();
    assert(notifications == 34 && capture.available() == 1);
    assert(capture.snapshot().gapMaxUs == 16000);
    capture.end();
    assert(!PDM.running && PDM.stops == 1);
    PDM.callback(); // A late callback must not dereference a former receiver.
    assert(notifications == 34);
    assert(other.begin(notify, &notifications));
    other.end();
    assert(!PDM.running && PDM.stops == 2 && interruptMask == 0);
    {
        AudioCapture scopedCapture;
        assert(scopedCapture.begin(notify, &notifications));
    }
    assert(!PDM.running && PDM.stops == 3);
    PDM.callback(); // Destruction unregisters the former owner.
    assert(notifications == 34);
    {
        AudioCapture partial;
        assert(partial.begin());
        PDM.readLimit = 2;
        PDM.readCalls = 0;
        for (int i = 0; i < 512; ++i)
            PDM.pending.push_back(i);
        PDM.callback();
        assert(PDM.readCalls == 256 && partial.available() == HOP_SIZE);
        assert(PDM.pending.size() == HOP_SIZE);
        PDM.readLimit = 0;
        PDM.callback();
        assert(partial.snapshot().missedBuffers == 1);
        PDM.pending.clear();
    }
}
