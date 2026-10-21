#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "feature_extractor.h"

extern bool failFftInitialization;
extern std::array<float, FRAME_SIZE> lastAppliedWindow;

int main() {
    FeatureExtractor extractor;
    assert(!extractor.begin(0.0f, 0));
    assert(!extractor.begin(-1.0f, 0));
    assert(!extractor.begin(std::numeric_limits<float>::quiet_NaN(), 0));
    assert(!extractor.begin(std::numeric_limits<float>::infinity(), 0));
    assert(!extractor.begin(1.0f, -129));
    assert(!extractor.begin(1.0f, 128));
    failFftInitialization = true;
    assert(!extractor.begin(1.0f, 0));
    failFftInitialization = false;
    std::array<std::int16_t, HOP_SIZE> samples{};
    for (const float scale : {0.09529872239f, 0.0001f, 1.0f}) {
        assert(extractor.begin(scale, 17));
        assert(!extractor.inferenceDue());
        std::uint32_t random = 7;
        for (int hop = 0; hop < 240; ++hop) {
            for (int i = 0; i < HOP_SIZE; ++i) {
                random = random * 1664525u + 1013904223u;
                samples[i] = hop < 75
                    ? 0
                    : (hop < 150 ? (i % 16 < 8 ? 30000 : -30000)
                                 : static_cast<std::int16_t>(
                                       static_cast<std::int32_t>(random >> 16) - 32768));
            }
            extractor.consume(samples);
            const bool expectedDue = hop >= NUM_FRAMES && (hop - NUM_FRAMES) % INFERENCE_HOP == 0;
            assert(extractor.inferenceDue() == expectedDue);
            if (extractor.inferenceDue()) {
                assert(std::fwrite(extractor.features().data(), 1, NUM_FRAMES * NUM_MEL, stdout) ==
                    NUM_FRAMES * NUM_MEL);
                extractor.markInferred();
                assert(!extractor.inferenceDue());
            }
        }
        // A periodic Hann has mean 1/2, peak 1, and a nonzero final coefficient.
        // These checks reject a FRAME_SIZE - 1 denominator independently of the feature digest.
        double windowSum = 0.0;
        for (const float coefficient : lastAppliedWindow) {
            windowSum += coefficient;
        }
        assert(std::abs(windowSum - FRAME_SIZE / 2.0) < 0.001);
        assert(lastAppliedWindow[0] == 0.0f);
        assert(std::abs(lastAppliedWindow[FRAME_SIZE / 4] - 0.5f) < 0.000001f);
        assert(lastAppliedWindow[FRAME_SIZE / 2] == 1.0f);
        assert(std::abs(lastAppliedWindow[3 * FRAME_SIZE / 4] - 0.5f) < 0.000001f);
        assert(std::abs(lastAppliedWindow.back() - 0.00003764908f) < 0.0000001f);
        extractor.reset();
        for (int i = 0; i < NUM_FRAMES; ++i) {
            extractor.consume(samples);
            assert(!extractor.inferenceDue());
        }
    }
    static_assert(
        !std::is_copy_constructible<FeatureExtractor>::value, "Descriptors cannot alias a copy");
    FeatureExtractor silence, tone;
    assert(silence.begin(0.09529872239f, 17));
    assert(tone.begin(0.09529872239f, 17));
    std::array<std::int16_t, HOP_SIZE> zero{};
    for (int hop = 0; hop <= NUM_FRAMES; ++hop) {
        silence.consume(zero);
        tone.consume(samples);
    }
    assert(silence.inferenceDue() && tone.inferenceDue());
    assert(
        std::memcmp(silence.features().data(), tone.features().data(), NUM_FRAMES * NUM_MEL) != 0);
    assert(silence.features().data() != tone.features().data());
    silence.reset();
    assert(!silence.inferenceDue() && tone.inferenceDue());
    // Tiny positive scales overflow the float quotient; conversion must saturate safely.
    assert(extractor.begin(std::numeric_limits<float>::min(), 0));
    for (int hop = 0; hop <= NUM_FRAMES; ++hop)
        extractor.consume(zero);
    assert(extractor.inferenceDue());
    for (const auto feature : extractor.features())
        assert(feature == -128);
    // Backlog can defer inference arbitrarily without an ever-growing signed counter.
    for (int hop = 0; hop < 1000; ++hop)
        extractor.consume(zero);
    assert(extractor.inferenceDue());
    extractor.markInferred();
    for (int hop = 0; hop < INFERENCE_HOP; ++hop) {
        extractor.consume(zero);
        assert(extractor.inferenceDue() == (hop == INFERENCE_HOP - 1));
    }
}
