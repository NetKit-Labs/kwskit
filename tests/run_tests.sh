#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_dir"
test_dir=$(mktemp -d /tmp/kws-tests.XXXXXX)
for test in audio_queue fft_power stream keyword_filter keyword_detector timing_stats; do
  c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined "tests/${test}_test.cpp" -o "$test_dir/$test"
  "$test_dir/$test"
  echo "PASS: $test"
done
python3 tests/device_action_test.py
echo 'PASS: device_action'
python3 tests/pipeline_test.py
echo "PASS: pipeline"
