# PDM read-error backoff

Arduino Zephyr Boards 1.0.0's PDM worker immediately retries a negative
`dmic_read` result at priority 3. A persistent failure can therefore starve the
lower-priority audio, control and diagnostic threads.

`stage_pdm_library` in [project.py](../../scripts/project.py) copies the verified
core's PDM source/header and library metadata to `.build/pdm-realtime/PDM`.
It adds `k_msleep(1)` before retrying a failed read. Successful reads and the
Arduino PDM API retain their original behavior. The exact patch context must
occur once; a core update that changes it stops staging for review. The installed
core remains intact, and builds explicitly select the staged library through
Arduino CLI's `--library` option. Its file hashes accompany each normal/stress
build manifest.

This is a project compatibility patch to Arduino's driver, not an upstream
Arduino release. It still uses the official packaged compiler, loader and APIs.
The driver's thread stack remains a one-time startup allocation; audio slab
buffers remain fixed-size pool storage. A delay requests a scheduling opportunity;
it does not guarantee device recovery or the execution time of a device call.

After `make setup`, run `make test-pdm-driver`. The test extracts the actual
worker body from both original and staged sources, compiles it against kernel
and microphone fixtures with sanitizers, and verifies:

- The unpatched worker fails the sleep-before-retry check.
- Transient failures and 1,000 persistent failures each sleep for 1 ms before retry.
- Successful reads dispatch callbacks; full queues release the slab block.
- A callback making no progress cannot cause an infinite leftover loop.

This is host fault injection, not physical microphone fault injection or a
whole-system timing proof. See [real-time limits](../../docs/realtime-review.md)
and [validation](../../docs/validation.md).
