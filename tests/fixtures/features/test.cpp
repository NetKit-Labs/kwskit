#include "feature_extractor.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <type_traits>

extern bool failFftInitialization;

int main() {
    FeatureExtractor extractor;
    assert(!extractor.begin(0.0f, 0));
    assert(!extractor.begin(-1.0f, 0));
    failFftInitialization = true;
    assert(!extractor.begin(1.0f, 0));
    failFftInitialization = false;
    std::array<int16_t, HOP_SIZE> samples{};
    for (const float scale : {0.09529872239f, 0.0001f, 1.0f}) {
        assert(extractor.begin(scale, 17));
        assert(!extractor.inferenceDue());
        uint32_t random = 7;
        for (int hop = 0; hop < 240; ++hop) {
            for (int i = 0; i < HOP_SIZE; ++i) {
                random = random * 1664525u + 1013904223u;
                samples[i] = hop < 75
                    ? 0
                    : (hop < 150
                              ? (i % 16 < 8 ? 30000 : -30000)
                              : static_cast<int16_t>(static_cast<int32_t>(random >> 16) - 32768));
            }
            extractor.consume(samples.data());
            const bool expectedDue = hop >= NUM_FRAMES && (hop - NUM_FRAMES) % INFERENCE_HOP == 0;
            assert(extractor.inferenceDue() == expectedDue);
            if (extractor.inferenceDue()) {
                assert(std::fwrite(extractor.data(), 1, NUM_FRAMES * NUM_MEL, stdout) ==
                    NUM_FRAMES * NUM_MEL);
                extractor.markInferred();
                assert(!extractor.inferenceDue());
            }
        }
        extractor.reset();
        for (int i = 0; i < NUM_FRAMES; ++i) {
            extractor.consume(samples.data());
            assert(!extractor.inferenceDue());
        }
    }
    static_assert(
        !std::is_copy_constructible<FeatureExtractor>::value, "Descriptors cannot alias a copy");
    FeatureExtractor silence, tone;
    assert(silence.begin(0.09529872239f, 17));
    assert(tone.begin(0.09529872239f, 17));
    std::array<int16_t, HOP_SIZE> zero{};
    for (int hop = 0; hop <= NUM_FRAMES; ++hop) {
        silence.consume(zero.data());
        tone.consume(samples.data());
    }
    assert(silence.inferenceDue() && tone.inferenceDue());
    assert(std::memcmp(silence.data(), tone.data(), NUM_FRAMES * NUM_MEL) != 0);
    assert(silence.data() != tone.data());
    silence.reset();
    assert(!silence.inferenceDue() && tone.inferenceDue());
}
