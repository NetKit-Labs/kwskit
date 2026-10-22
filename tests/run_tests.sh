#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_dir"
suite=${1:-all}
case "$suite" in
    unit|all) ;;
    *)
        echo "Usage: $0 [unit|all]" >&2
        exit 2
        ;;
esac
test_dir=$(mktemp -d /tmp/kws-tests.XXXXXX)
for test in audio_queue fft_power keyword_filter keyword_detector keyword_event timing_stats; do
    c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined "tests/${test}_test.cpp" -o "$test_dir/$test"
    "$test_dir/$test"
    echo "PASS: $test"
done
python3 tests/device_action_test.py
echo 'PASS: device_action'
python3 tests/audio_capture_test.py
echo 'PASS: audio_capture'
python3 tests/feature_extractor_test.py
echo 'PASS: feature_extractor'
python3 tests/keyword_model_test.py
echo 'PASS: keyword_model'
python3 tests/telemetry_test.py
echo 'PASS: telemetry'
python3 tests/buffer_contract_test.py
echo 'PASS: buffer_contract'
echo 'PASS: unit suite (12 checks)'
if [ "$suite" = unit ]; then
    exit 0
fi
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined tests/stream_test.cpp -o "$test_dir/stream"
"$test_dir/stream"
echo 'PASS: stream'
python3 tests/pipeline_test.py
echo "PASS: pipeline"
python3 tests/build_tooling_test.py
echo 'PASS: build_tooling'
python3 tests/zephyr_startup_test.py
echo 'PASS: zephyr_startup'
echo 'PASS: complete regression suite (16 checks, including unit tests)'
