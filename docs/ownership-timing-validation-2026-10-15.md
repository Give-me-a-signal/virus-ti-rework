# Ownership and marker timing validation — 15 October 2026

These results concern the draft ownership/recall and dependent timing changes,
not a replacement for the published beta. Synchronized timing remains disabled
by default until its hardware acceptance tests pass.

## Configuration

- Virus TI Desktop; M1 Pro; macOS 26.2 (25C56), native arm64.
- AURA CoreMIDI driver restored after the separate TI Revive comparison.
- Device capture at 48 kHz; original 44.1 kHz nominal setting recorded for
  restoration after testing. No USB reset or firmware operation.
- Release build, macOS 12 deployment target; existing pinned VST3 SDK and
  AudioUnitSDK. AU embeds its own VST3 and restoration helper.
- Firmware version and physical USB adapter/hub model not recorded. TI2/Snow,
  Intel, Windows and Linux are not established by these results.

## Reproduced results

| Check | Result and scope |
| --- | --- |
| RAM project recall | **Pass**, actual VST3 bundle at `bfc72a3`, 48 kHz/256 frames, immediate timing. Ten cycles of a reversible supported cutoff edit followed by whole-project restore. All sixteen sound payloads and multi routing matched after every cycle, using the existing documented normalization exceptions. |
| Competing format | **Pass**, AU Connect refused while VST3 owned the physical TI. Losing AU had zero capture callbacks, zero MIDI sends and no snapshot directory. This does not yet establish continuity during a sounding timing sequence. |
| Manual disconnect | **Pass**. Independent file comparison found all eighteen original edit buffers byte-identical after restoration. |
| Normal host exit | **Pass**. Fresh connection, normal host shutdown and helper restoration; all eighteen original edit buffers byte-identical. |
| Capture health during recall run | **Pass**, no additional capture errors, FIFO underruns or overruns. |
| Repeated hardware state saves | **Pass**, all ten recalled project-state files byte-identical to the 40,871-byte baseline; no appended records or size growth. This does not complete the separate hardware-to-UI parameter mapping work. |
| Protocol audit | **Pass** for manual and helper recovery traffic: only the restricted known read, edit-buffer, note/control and USB-mode messages. No stored-bank writes, firmware transfer or USB resets. |
| Offline processor/editor/capture/recovery/ownership suites | **Pass**, 15/15 at `351104c`; actual-bundle scan and repeated refused-connection tests included. |
| Steinberg validator | **Pass**, 47/47 at `aa8e8f8`; no explicit Connect. |
| Address/undefined sanitizers | **Pass**, capture, recall-policy and timing suites. |
| Recording analyzer | **Pass** on synthetic references, including wrong latency, missing/extra onsets, jitter, drift and each return. This validates the analyzer, not device timing. |
| First VST3 timing measurement | **Pass**, 48 kHz/256, return 1; 256 onsets, mean error −0.0002 ms, jitter SD 0.0026 ms, drift −0.03 ppm. |
| First AU timing measurement | **Pass**, 48 kHz/256, return 2; 256 onsets, mean error −0.0252 ms, jitter SD 0.0469 ms, drift 0.18 ppm. Competing VST3 refused during playback, with no missing onsets or new capture errors. |

Both initial timing cases used the native binary built at `aa8e8f8`, reported
3,104 samples of latency, and passed helper restoration and the restricted MIDI
traffic audit. There were no additional late events or capture errors during
the measured sequences. Setup's long patch read/write operations had discarded
27 late clock events in each case before the measurement baseline; this is
not a claim of zero late events throughout initialization. Unselected returns
and the final release tail were silent. The captured hardware Init edit buffer
was used only privately. These are short bench measurements, not a long-session
or arbitrary-patch timing guarantee.

## Outstanding acceptance

Marker-pattern detection is separate from proof of the 48 kHz clock relationship.
The first physical timing measurements passed. The full AU/VST3 matrix at
44.1/48/96 kHz with 64/256/512-frame blocks, all three returns, sounding ownership
continuity and current-build Logic/Bitwig save/reopen remain pending.

The target is no missing/extra notes or new capture errors, corrected mean error
within 1 ms, jitter standard deviation within 0.5 ms and drift within 10 ppm.
Threshold measurements include the captured patch's attack and oscillator
behaviour; they cannot silently subtract a fitted latency offset to claim a pass.

Raw patches, projects, recordings, identifiers and recovery directories remain
private. Tests use owner-captured RAM sounds, not redistributed factory fixtures.
No firmware, stored banks or automatic bank synchronization are involved.

Ableton saving remains deferred because authorization was declined. LiveProfessor
is deferred at the owner's request. Earlier DAW results for the released Rework
beta or TI Revive do not validate these new changes.
