# C++20 contracts and ownership

The firmware, host inspector and native tests use C++20. Stateful modules are
noncopyable owners with fixed storage; pure events/results/statistics are ordinary
copyable values. Namespaces group stateless helpers and internal implementation
types. No application class hierarchy or dynamic polymorphism is needed.

## Fixed-size interfaces

`AudioHop` is `std::array<int16_t, 256>` and `FeatureTensor` is
`std::array<int8_t, 1220>`. References preserve extents when capture, extraction
and inference exchange data. The ring's `pop` rejects a requested count larger
than its destination before consuming anything. `fftPower` binds the transform
length to exactly `N/2 + 1` power outputs and rejects odd transform lengths.
`std::has_single_bit` enforces a nonzero power-of-two ring capacity. Compiler
probes reject wrong extents, raw-pointer ring reads and copying resource owners.

`std::copy` expresses frame/context movement; left-shift destinations precede
the source ranges. The consumer discards backlog by advancing the atomic tail
to a head snapshot; later publications remain queued. Timestamp arithmetic widens
the backlog product to 64 bits before multiplying by 1,000,000.
`KeywordEvent` must remain trivially copyable for `k_msgq` byte-copy transport.

## Objects and lifetimes

| Owner | State and lifetime |
| --- | --- |
| `AudioCapture` | Single active callback owner, PDM staging buffer, ring and counters; destruction stops capture and unregisters ownership |
| `FeatureExtractor` | Fixed frame/FFT/mel/context arrays and descriptors pointing into them; copying would invalidate ownership |
| `AudioPipeline` | Borrows capture, extractor and model by reference; owns hop, timing and recovery state |
| `KeywordModel::Runtime` | Static aligned arena, resolver, tensors and placement-constructed interpreter |
| `DeviceControl` | Initialized GPIO action and logical LED state |
| `Telemetry` | Fixed 640-byte buffer, offsets and report timestamp |
| Private `Application` | Static kernel objects, aligned stacks, score storage, modules and shared diagnostics |

The function-local application is constructed after Arduino globals. Its cleanup
stops capture before aborting consumers. Repeated successful startup creates no
duplicate resources. Interpreter construction uses aligned member storage and
explicit destruction rather than an application heap allocation. Framework/driver
startup may still allocate. Resource ownership does not make concurrent use of
the model or extractor safe; their thread ownership remains exclusive.

## Scoped synchronization and API boundaries

Capture counters use always-lock-free `std::atomic<std::uint32_t>`; the callback
owner uses an always-lock-free atomic pointer. Counter updates have one writer
and use bounded loads/stores. IRQ masking supplies a coherent multi-field
snapshot on this single-core target; atomics supply C++ data-race safety.

`InterruptLock` saves the Zephyr IRQ key and restores it, preserving nested masks.
`MutexLock` tries `k_mutex_lock` with `K_NO_WAIT` and unlocks only when acquired.
`SchedulerLock` balances recursive Zephyr scheduler locks and leaves interrupts
enabled around the final event check and initialized LED write. Guards do not
permit waits or blocking device operations within their scopes.

Thread entry functions and the PDM C callback route context pointers to stable
owners. Required existing objects use references; null-state runtime tensors,
optional FlatBuffer metadata and inactive callback owners appropriately use
pointers. CMSIS-DSP receives raw `.data()` buffers only at its API boundary.
Its matrix API requires a mutable coefficient pointer although multiplication
reads the flash table; the confined `const_cast` addresses that API signature.
Serial writes use explicit byte views and execute only in lower-priority main,
after releasing all application locks. Bounded `std::snprintf` avoids introducing
an allocating formatting dependency.

C++20 designated initialization names event/snapshot members, `constinit` sets
the callback owner at static initialization, and `std::numbers::pi_v<double>`
provides the periodic Hann constant. The host inspector uses `std::set::contains`
and `std::string_view`. The shared header rejects older language standards.
[Unit/regression tests](testing.md) protect these contracts and
[source style](source-style.md) defines their maintained formatting.
