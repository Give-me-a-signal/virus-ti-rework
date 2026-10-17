# TI Revive findings compared with Rework

Source: [issue #5](https://github.com/Give-me-a-signal/virus-ti-rework/issues/5).
TI Revive source reviewed at `21b83e6f9c05497d8331023e9ff6845361c2d36c`;
the previously tested release was 0.8.1. Reports below are collaborator evidence,
not additional reproduced hardware results for Rework.

| Finding | Rework assessment and action |
| --- | --- |
| 1. Serialized dump requests | Already serialized on the service worker. Add a request mutex, three-second deadline and 1.5-second late-reply drain before bounded read retries, shared with recovery. Never retry patch writes automatically. |
| 2. Fingerprint bank synchronization | Useful future feature; this beta intentionally uses edit-buffer capture and user-imported files. No automatic bank sync or stored-bank access added. |
| 3. SysEx delays notes | Whole-project recall now suspends performance delivery and audio readiness, panics all parts and invalidates old queued events. Saving still makes serialized hardware reads; it is not a guarantee of uninterrupted timing during a hardware snapshot. |
| 4. Multi settling | One 250 ms minimum after completed multi transmission, including before reference controls in recovery. Validate multi sound/routing as well as its checksum. |
| 5. Duplicate global records | Not applicable to Rework's fixed versioned state structure. Retain existing readers/IDs and repeated round-trip tests. No incoming global-record append path is introduced. |
| 6. Marker clock | Integrate separately with 48 kHz evidence gating; preserve the six-channel capture and existing resampler. Do not transplant 44.1 kHz constants. |
| 7. Measured timing | Use as a comparison method, not a result for Rework. Record our own event counts, latency, jitter and drift before enabling synchronization by default. |
| 8. Exclusive ownership | Existing shared flock lease survives host-to-helper handoff. Encapsulate it with close-only RAII; acquisition failures close their descriptor and cannot release another owner's lease. Add contention/inheritance tests and keep scanner paths dormant. |
| 9. Repeated USB resets | No USB reset added. Recovery uses identity checks and bounded read/open retries. A failure stops the test rather than triggering resets. |
| 10. Microphone permission | CoreAudio capture requires host permission; installation/testing instructions already cover it. A silent signal alone does not prove permission failure. Keep permission-denied/missing capture distinguishable from a silent patch in test reports. |

## Verification boundaries

Offline tests cover lease contention/inherited ownership, multi comparison,
the settling barrier, delayed replies and drained retry limits, existing state
compatibility and recovery failure continuation. Hardware validation is recorded
separately; compilation and synthetic tests do not establish TI2 compatibility.
Firmware transfer and stored-bank writes remain outside the MIDI allowlist.

Manual disconnect and the independent helper now run the same restoration
sequence. It restores the 16 part edit buffers, Single and multi sound/routing;
it sends the established Single-mode/USB normalization. This does not assert a
complete snapshot of arbitrary global settings.
