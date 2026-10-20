// Compiler contract probe: valid buffers.
#include "common.h"

int main() {
    AudioCapture capture;
    FeatureExtractor extractor;
    auto &model = KeywordModel::instance();
    AudioHop hop{};
    FeatureTensor features{};
    std::uint32_t timestamp = 0;
    float probability = 0;
    capture.readHop(hop, timestamp);
    extractor.consume(hop);
    model.predict(features, probability);
    std::array<float, 512> packed{};
    std::array<float, 257> power{};
    fftPower(packed, power);
    AudioQueue<8> queue;
    std::array<std::int16_t, 2> pair{};
    queue.pop(pair);
    static_assert(
        !std::is_copy_constructible<AudioPipeline>::value, "pipeline is a single consumer");
    static_assert(!std::is_copy_constructible<DeviceControl>::value, "device owns output state");
    static_assert(!std::is_copy_constructible<Telemetry>::value, "telemetry owns an output stream");
    static_assert(!std::is_copy_constructible<AudioCapture>::value, "capture owns a receiver");
    static_assert(
        !std::is_copy_constructible<FeatureExtractor>::value, "extractor owns descriptors");
}
