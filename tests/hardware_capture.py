"""Read and validate Nano telemetry. Requires pyserial and USB device access."""
import argparse
import json
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='/dev/cu.usbmodem11201')
parser.add_argument('--seconds', type=int, default=30)
parser.add_argument('--output', default='/tmp/kws-hardware.json')
args = parser.parse_args()
records = []
with serial.Serial(args.port, 115200, timeout=1) as port:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        line = port.readline().decode(errors='replace').strip()
        if line:
            print(line, flush=True)
        if line.startswith('ms=') and 'led=' in line:
            records.append(dict(item.split('=', 1) for item in line.split()))
Path(args.output).write_text(json.dumps(records, indent=2) + '\n')
assert len(records) >= 3, 'Not enough telemetry records'
for row in records:
    assert int(row['dropped']) == 0, row
    assert int(row['missed_buffers']) == 0, row
    assert int(row['errors']) == 0, row
    assert int(row['queue_max']) < 8192, row
    assert int(row['audio_age_us']) < 100000, row
for before, after in zip(records, records[1:]):
    assert int(after['samples']) > int(before['samples']), 'Audio stalled'
    assert int(after['inferences']) > int(before['inferences']), 'Inference stalled'
print('PASS: live acquisition and inference, no reported drops or errors')
