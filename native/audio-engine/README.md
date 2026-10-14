# Native USB audio engine, offline pass 3

This module adds a real asynchronous engine implementation and a macOS 12+ IOUSBHost adapter. **Neither has streamed to a Virus.** The adapter compiles and links for Apple silicon, and its translation/lifecycle is tested with Objective-C test doubles. Nothing here identifies the actual Virus alternate setting or proves that the device uses this isochronous path.

## Implemented

- Fixed pool of at most eight requests, at most eight service intervals each; all C++ engine storage is allocated at construction. `start`, `pump`, `complete` and `stop` are allocation-free, checked by an allocation-failure guard during lifecycle tests.
- Separate capture/playback queues, descriptor-derived packet capacities, PCM16/24 encoding/decoding, whole-packet silence on playback underrun and whole-packet discard on capture overrun. Capture may have variable packet sizes. Successful short playback is counted as a discontinuity. Error payload is discarded.
- Explicit idle/running/draining states, partial-start failure cleanup, asynchronous cancellation, terminal-callback ownership, and generation + submission tokens that reject stale/duplicate completions. Completed requests wait for earlier transfers in the same direction, preserving sample order even when callbacks arrive out of order.
- Checked packet bounds/alignment, descriptor direction/type/interval/capacity, configuration/queue consistency, service-counter overflow, frame/byte counts, disconnect/stall failure, and cumulative counters. Callbacks are never treated as permission to reuse some other live request's buffer.
- Distinct service-interval ordinals and optional monotonic host nanoseconds. Capture records the most recently committed packet's timestamp and ordinal, with explicit validity. This is diagnostic timing, **not a timestamped audio ring or a DAW sample-clock mapping**.

## Actual macOS backend

`iousbhost_backend.mm` submits `IOUSBHostIsochronousTransaction` arrays through `enqueueIORequestWithData:transactionList:transactionListCount:firstFrameNumber:options:error:completionHandler:`. It verifies the supplied pipes' descriptors, uses the transaction's explicit offset, and retains preallocated `NSMutableData` backing storage for every outstanding request. Bounded copies bridge that storage to the engine's buffers; this is not a zero-copy path. The implementation does not allocate its buffers or completion blocks during streaming, but allocations and scheduling inside Foundation/IOUSBHost are outside that guarantee.

The caller must supply **already opened, correctly selected data pipes**, with their interfaces using the **same serial dispatch queue**. Engine control, adapter calls and completions all run on that queue; only `AudioFrameQueue` producers/consumers run concurrently on audio threads. The adapter binds once to its engine, calls `complete` on terminal callbacks, and then calls `pump` to replenish the pool. A common serial executor prevents inline reentrancy. No concurrent direct `Engine` method calls are supported.

Full-speed `first_service` maps directly to a nonzero absolute USB frame number. High-speed operation requires exactly eight packets per request and a service ordinal divisible by eight; the OS frame number is `first_service / 8`. The owner obtains the current controller frame and chooses an experimentally verified future lead. Zero/ASAP semantics are rejected; there is no guessed scheduling lead in this module. Only USB2 full/high-speed isochronous data endpoints with `bInterval=1` are supported. Feedback-only and reserved-usage endpoints are rejected. Actual endpoint/alternate/sample-rate selection remains external and unverified.

Cancellation uses **asynchronous, pipe-wide abort**, coalesced to one successful abort per direction. Each accepted request still requires its own terminal completion; abort failure is surfaced in `last_error()` and does not free buffers or manufacture completion. Do not destroy the engine/backend/queues or interface owners until all real callbacks have drained and `state() == idle`. Destruction while buffers remain owned terminates deliberately to prevent use-after-free. A stalled or disconnected stream stops; automatic reopen/reselection is not implemented.

The SDK's `IOUSBHostTime` is absolute host ticks; conversion uses `mach_timebase_info` and a wide intermediate with overflow validation. These are observed transaction **completion** timestamps and may repeat under interrupt latency. They must not be presented as precise audio sample timestamps. USB data-underrun status is normalized as a short transaction, then the engine checks byte bounds/alignment; other errors remain explicit.

## REA behavioral model

`RecoveredFrameObserver` reconstructs the supported 44.1/48 kHz full/high-speed branches of these saved kernel decompilations:

| Evidence | Behavior modeled |
|---|---|
| `analysis/decompiled-pass2/AccessVirusTI_0x1a746_PGFramePatternObserver__init.c`, `ev_55a08a0107ad87794625361ed97a099a90aa85b5bcf6a21f0987ee08608d5109` | 16/128-entry initial table; 44.1k FS 45 at index 0 and 10, otherwise 44; HS alternating 5/6 with increments at 0 and 80; 48k constant 48/6 |
| `analysis/decompiled-pass2/AccessVirusTI_0x1af08_PGFramePatternObserver__onRtsFrame.c`, `ev_0efc8852bbdb96b9c54f67edaf205255ce03f9c3b62e216daddc270256bef9ff` | Input tolerance; initial-table substitution on invalid counts; write cursor wrap; synchronization regain at the previous error cursor; reset error count |
| `analysis/decompiled-pass2/AccessVirusTI_0x1b602_PGIsocRB__initWrite_Std.c`, `ev_85ee31a9b4023eb7d7f3b39e7e33f09ae02257834b2149ee6f545dfddea02208` | Separate output cursor reads the learned table cyclically |

Kernel SHA-256: `7308f591d634625acfaa0019f218af99e04cf936b4dfe18227a0bbf28a67e539`.

The model uses the generic FS ±2/HS ±1 tolerance. The decompiled special VID/PID value `0x6000133e` that selects zero tolerance at 48/96 kHz is **not implemented**; device identity and applicability must be resolved before using this model with hardware. The model also excludes the full-frame feedback callback, other rate branches and any inferred phase/latency coupling. **It is not wired into the engine.** Engine playback uses the existing nominal floor/carry schedule, which will drift relative to an independent device clock. Repeating the recovered 16/128-entry initial tables forever is not a valid long-term 44.1k clock model either.

## Validation

Run from the project root:

```sh
sh native/audio-engine/build_and_test.sh
```

The script uses RTK when available and falls back to the compiler/tools when absent. On 11 October 2026 all checks passed on arm64:

- Normal and ASan/UBSan engine builds: startup rejection at each of four submission positions, ownership/drain/restart, same-generation slot reuse and old-generation callbacks, malformed completion counts and packet lengths, overflow, short/error packets, underrun/overrun, PCM16/24, FS/HS, and explicit observer vectors.
- All 24 completion orders repeated 100 times (2,400 batches / 9,600 terminal callbacks), checking capture sample order; all 120 stop/completion interleavings. These test serialized race orderings, not unsupported simultaneous calls into `Engine`.
- Sanitized IOUSBHost adapter build and selector tests: FS/HS OS-frame mapping, transaction offsets, copy/status/timebase conversion, refill, abort/drain, enqueue errors, malformed completion lengths and abort failure. The test supplies NSObject stand-ins for pipes: **no IOUSBHost device or interface object is constructed, and no USB I/O occurs**.

The existing transport-core suite independently covers concurrent SPSC queue traffic. Thread sanitizer was not run for this module; the engine itself requires one executor.

Local SDK references used: `IOUSBHost.framework/Headers/IOUSBHostPipe.h` (isochronous enqueue and asynchronous abort), `IOUSBHostDefinitions.h` (transaction layout/status/host timestamps), `IOUSBHostObject.h` (frame/microframe domains), and `IOKit/usb/USB.h` (data-underrun and transport status definitions). These were read from `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/`.

## Remaining before meaningful hardware audio tests

Recover/measure actual interfaces, transfer type, sample packing, sample-rate control sequence, queue depth and clock feedback; open interfaces on a shared serial queue and select proven alternates; supply the correct pipe owners and future controller frame; then validate capture first and bounded playback. CoreAudio device publication, timestamped host audio delivery, long-run clock correction/resampling, dropout concealment and connection/restart handling remain separate work. The module does not expose a CoreAudio device or yet connect USB audio to the VST3 processor.

Queue contents are host-owned and survive engine stop/restart. The owner must drain or replace queues at a discontinuity before restarting. A failed playback submission may already have consumed its queue samples; there is no rollback into an SPSC consumer. All errors/overruns/underruns need host policy before a user-facing driver is ready.
