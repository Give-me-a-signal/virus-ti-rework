# AU state validation — 15 October 2026

This records development findings, not a new compatibility claim for the
published beta. The ownership/recall PR must remain draft until live Logic
save/reopen has passed.

## Reproduced

- TI Desktop, M1 Pro, macOS 26.2 (25C56); Logic Pro 12.4 (6707), native AU,
  48 kHz project, 128-frame I/O setting, Medium process buffer.
- A real-time eight-second stereo bounce contained all sixteen expected notes
  from a synthetic MIDI clip. The sound was an owner-captured RAM Init edit
  buffer with a small reversible cutoff edit. No factory fixture was bundled.
- Each completed normal Logic exit restored all eighteen RAM buffers
  byte-for-byte against the original pre-test capture. Recovery traffic passed
  the restricted protocol audit. No firmware, stored-bank operation or USB
  reset was used.
- Loading/scanning the AU without Connect produced zero capture callbacks and
  zero MIDI sends. Offline parsing does not acquire the device lease.

## State notification adaptation

The pinned Steinberg wrapper exposes `IComponentHandler`, but not
`IComponentHandler2`. Consequently the controller's non-parameter `setDirty`
notification could not reach an AU host. The build now generates a local wrapper
copy with that interface and forwards it to the AU ClassInfo property listener.
It also propagates failed processor saves/restores instead of reporting success
with an empty or rejected processor state. The SDK checkout, AU identity, VST3
class/parameter IDs and state format remain unchanged.

A synthetic test fails at the missing notification against the original wrapper
and passes with the adaptation. It stages a checksum-valid project-owned patch,
observes the ClassInfo notification, rejects a corrupt CRC, and retains the full
payload through ten AU property round trips. It asserts no device lease,
capture callbacks or MIDI sends. All fifteen local suites and strict `auval`
passed with the adapted wrapper.

## Live save/reopen has not passed

An offline staged sixteen-part state was successfully saved into Logic's project
file (40,871 processor bytes). In the connected test, fresh API state captures
contained the requested sixteen sounds, routing and reconnect flag, but Logic's
saved project retained the earlier state/reconnect setting. An earlier reopen
restored controls with empty patch payloads and did not reconnect. The
notification adaptation alone has **not** resolved this live save path.

Read-only `state_revision` and `state_capture_count` diagnostics distinguish
host snapshot requests from missing dirty notifications. Hardware snapshot
latency, host caching and asynchronous save completion still need investigation;
a timeout is a hypothesis, not a reproduced root cause. Do not claim Logic
project recall from the successful bounce, native state API or `auval` result.

The first extended idle/dialog interval also accumulated 1,189 FIFO overruns;
the subsequent bounce added none and had no underruns or capture-format errors.
This is not an error-free long-session validation. Raw patches, project files,
recordings and device identifiers remain private.
