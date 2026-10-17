# Snow four-part recall — guarded implementation

This follows the [detection audit](device-model-audit.md) and [issue #8](https://github.com/Give-me-a-signal/virus-ti-rework/issues/8).
**Snow remains unverified on physical hardware.** Current PR builds let the
normal **Connect TI** action select the four-part profile without a separate build
flag or opt-in button. Four-part data handling has offline and
[local firmware-emulator coverage](snow-emulator-validation-2026-10-15.md).
Stable current-format originals and recovery-helper readiness must validate before
RAM edits. See the [testing instructions](SNOW_TEST_BUILD.md) for the distinction
between current PR builds and the unchanged published beta/earlier preview.

## Implemented behavior

- Snapshot parts 0–3 plus separate Single selector 64 and opaque Multi. Validate
  every original before publishing six buffers and a model record. TI retains
  sixteen parts/eighteen buffers.
- Store `snow4`/`ti16` in versioned `before-device-profile.txt`. Reject unknown
  records, missing/corrupt originals, wrong addresses and extra inactive parts.
  Legacy directories without a record retain the sixteen-part contract.
- Keep v3, class/parameter IDs, normalized values and sixteen serialized slots.
  Four-part projects leave slots 4–15 empty. Reject populated inactive slots
  before project-recall commands.
- Recall opaque Multi, settle at least 250 ms, compare readback, then load only
  active populated parts. Stop active held notes and suppress/discard queued
  performance events during initialization/whole-project recall.
- Bound snapshots, loads, routing, MIDI and volume replay to active parts.
  Preserve packet envelopes, field strides and reserved/inactive bytes.
- Share model-bounded manual/helper restoration and preflight. Continue bounded
  readback evidence after a failed part write, then report the error. Snow
  comparisons are strict; TI normalization exceptions do not automatically apply.
- Both build variants arm Snow recovery from a complete validated snapshot.
  Connect records the Snow session using the existing sidecar format; the helper
  refuses a missing/invalid record. This is a recovery integrity check, not an
  extra user opt-in or a claim of hardware validation. Snow format/stability
  checks are model-based and cannot be disabled by omitting the session flag.
- Both variants always use the restricted Snow MIDI validator. Only four parts,
  Single/Multi modes 0/1, current-format RAM patches and USB1/2 stereo routing are
  allowed; TI-only globals, mode 3 and inactive channels/buffers remain blocked.

The editor retains its layout and the host retains existing buses/parameters.
The editor disables parts 5–16 and marks USB3 unavailable. The host retains its
existing buses/parameters for compatibility, with the third Snow return silent.
Physical returns and editor behavior still need testing on a real Snow.

## Packet evidence and limits

Static inspection of the owner's 5.1.7.00 `remotedevicePR.xml`, whose root names
Virus TI Snow, declares **524-byte Single/part** dumps (selector 8, checksums
265/522) and **267-byte Multi** (checksum 265 over bytes 5–264, terminator 266).
Do not shrink packets for four parts. Multi declarations retain sixteen-wide
fields: MIDI `73+p`, level `153+p`, output `185+p`, flags `249+p`.

XML SHA-256: `29c980aa97db074ae9eb9c51d8971481799e2ee48e99ae9256dd46d429e8001f`;
Single lines 9142–9597, Multi 9599–9899. This 2012 declaration is **not a Snow
wire capture**; precedence of original compiled definitions remains unresolved.
No vendor resources, implementation, manuals or personal dumps are redistributed.

Bank/Patch controls 32/33 are declared; serialized offsets `41+p`/`57+p` are not
established by this Snow declaration. Offline Snow restoration does not emit
guessed part-reference controls. TI keeps its observed sequence. XML declares
USB stereo values 10/13/16, while the manual documents only USB1/2; a third Snow
return remains unresolved.

The owner's Snow Reference, printed page 117, establishes four parts;
SHA-256 `990e72e7922d99efe54f2f9dc747f035b115b57e247b02ff647d49c2b270071c`.
Six originals including selector 64 is an implementation model requiring
physical confirmation, not an observed enumeration of Snow buffers.

## Validation and acceptance

Synthetic tests cover four/sixteen parts, six/eighteen buffers, model records,
legacy directories, inactive/missing/corrupt parts, fail-before-write preflight,
settling, failed writes, sound/routing mismatches and ten repeated recalls.
Every generated command is checked against the existing RAM-only MIDI boundary.
Portable tests run normally and with ASan/UBSan. The native suite checks that
the actual helper refuses a missing session record, and arms/completes a valid
Snow snapshot in both build variants without opening hardware. Discovery tests
select Snow with the ordinary connection path and verify all TI/TI2 aliases still
select sixteen parts. Every generated Snow test command passes the restricted
Snow validator, including recall and restoration.
Processor/controller tests perform ten four-part v3 state round trips with
constant length, preserved sound/routing and empty inactive slots.
See the PR for exact build, CI, validator, AU and packaging results.

Physical hardware acceptance remains outstanding:

1. Collect [read-only diagnostics](HARDWARE_TESTING.md#read-only-diagnostics-for-snow-and-other-unverified-models).
2. Establish USB/MIDI identity, capture stream, returns and selector 64; capture
   and validate all originals before any RAM write.
3. Validate mode/USB initialization, routing, part references, flags and recovery.
   Connection uses declared Snow Multi mode 1 and avoids the TI-only USB global
   normalization command. Keep support marked unverified until tested on hardware.
4. With an owner and reviewed bounded test build, check reversible edits, ten
   recalls, inactive-channel refusal, manual disconnect and helper recovery with
   full comparisons. No firmware, bank writes or USB resets.
5. Repeat TI Desktop regressions after the shared refactor, reconcile the
   separate ownership/recall PR, and test AU/VST3 host save/reopen.

Offline passes do not establish physical Snow support or new TI hardware results.
