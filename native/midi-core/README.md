# Native MIDI/control groundwork

This is a portable C++17, header-only module for the replacement engine. It does
not open CoreMIDI, claim a USB interface, contact the synth, or write patch data.

`include/midi_core.hpp` provides:

- A fixed-capacity MIDI 1.0 byte-stream parser, with channel running status,
  system common messages, real-time interleaving, and bounded SysEx assembly.
- Explicit error events for undefined status bytes, stray data, interrupted or
  unfinished messages, invalid buffer pointers, and oversized SysEx. Incomplete
  messages are never delivered as complete messages. Oversized SysEx is discarded
  until its terminator or a new non-real-time status; real-time events still pass.
- Checked CC and poly-pressure builders, and the seven short bank-write SysEx
  layouts declared by the Desktop `remotedevice.xml` shipped in the package.

Callbacks receive transient byte views and must copy if retaining data. The
parser does not allocate or lock, but the callback's behavior belongs to its
caller. A parser has one owner and callbacks must not reenter it. Each feed call
accepts a caller-defined timestamp; complete events retain the timestamp of their
first actual byte. Running-status events use the first data byte's timestamp.
`finish()` diagnoses incomplete input and resets all state; a buffer boundary
alone does not finish a stream. Maximum SysEx size includes F0 and F7. This code
does not transform Note On velocity zero into Note Off.

## Evidence and limits

The MIDI status lengths and real-time interleaving follow the
[MIDI Association message summary](https://midi.org/summary-of-midi-1-0-messages).
Reserved status handling, bounded recovery, and callback interfaces are choices
for this new implementation, not recovered vendor behavior.

The bank encoders reproduce these XML declaration bytes:

```text
F0 00 20 33 01 <raw device ID> <bank command> <raw part> <controller> <value> F7
```

Bank commands are C=6E, D=6F, A=70, B=71, E=72, F=73, G=74 (hexadecimal). XML
`tag` attributes use decimal, confirmed by the named bank commands 110–116 and
literal values above 99 in the same file. No checksum is present in these short
templates. This does not settle checksum rules for the three larger dump formats.
The APIs require caller-supplied raw device and part fields. They do not infer
whether a user-facing part number, MIDI channel, broadcast ID, or global part
sentinel is appropriate. Range validation here is the MIDI data-byte range, not a
claim that every value is meaningful to the instrument. Device behavior and
runtime use of XML versus compiled definitions remain unverified.

`Filter 1/Cutoff` is declared as CC controller 40, range 0–127, default 127.
Selecting a MIDI channel and synth operating mode is the caller's responsibility.
Other mode-dependent definitions can share a controller; the encoders do not
resolve conditions or authorize all catalog entries for simultaneous use.

Source file SHA-256:
`817c444db3a3b089bf09b82fd0391be734057afd24ca34ca272491ccb9152944`.
`scripts/verify_xml_templates.py` checks the original packaged source, its hash,
all nine short template structures, and the Cutoff definition on every test run.
No vendor source code is included in this module.

## Public USB MIDI packet codec

`include/usb_midi1.hpp` implements four-byte packet decoding, whole-transfer
validation, fragment encoding, and complete-message/SysEx encoding. Cable IDs
remain explicit; a caller-supplied cable mask rejects unavailable routes.
The stateless decoder returns fragments. Keep a separate `StreamParser` for each
cable to assemble messages without mixing cables. Input/output buffers must not
overlap. Validation failures leave outputs and `written` unchanged.

Packet layouts follow [USB-IF MIDI 1.0, section 4](https://www.usb.org/sites/default/files/midi10.pdf).
CIN F supports any individual MIDI byte; it is not limited to real-time messages.
SysEx endings select CIN 5/6/7 according to remaining bytes. The encoder zero-fills
unused packet bytes. Complete-message encoding accepts explicit status bytes and
complete SysEx; split interleaved real-time messages with the stream parser first.

The recovered [public receive function 0x109a0](../../analysis/decompiled-pass3/midi-0x109a0.c),
evidence `ev_00fd7c1ff49dfd4016077d8558f6ed691c74180edf67bc078e988d5cc284a981`,
corroborates the public cable/CIN layout, MIDI payload lengths, four-byte stepping,
and delivery through `MIDIPacketListAdd` / `MIDIReceived`. The replacement rejects
partial transfers, reserved CIN 0/1, mismatched statuses, malformed fragment
content, nonzero padding, and unavailable cables. This strict rejection policy is
new; the original clamps cable IDs and skips reserved CINs. Whole-transfer decode
validates every packet before writing any event. SysEx fragment sequencing remains
the downstream parser's responsibility. Hardware compatibility of strict padding
validation still needs checking against actual captures.

Public framing does **not** prove endpoint addresses or private control semantics.
Do not route the public packet encoder to private endpoints. No CoreMIDI service
is exposed yet.

## Private timing marker

`include/private_control.hpp` isolates one recovered private extension:
`53 F5 <low seven bits> <high seven bits>`. It encodes/decodes an explicit raw
14-bit timing value. This is not a public MIDI message: the public packet codec
rejects this CIN-3/F5 marker, and the byte-stream parser diagnoses reserved F5.
(Standard CIN F still permits any unparsed byte, including F5.) No device-clock
units, epoch, wrap scheduling, reset behavior, or event ordering is implemented.
All 16,384 raw values roundtrip in tests and remain rejected by the public codec.

The common-bundle [timing function 0xebd4c](../../analysis/decompiled-pass3/common-0xebd4c.c)
constructs F5 and two seven-bit pieces, then calls the
[private encoder 0xeacc8](../../analysis/decompiled-pass3/common-0xeacc8.c) with route
5. These support the exact four-byte marker. Evidence IDs:
`ev_664e49deda4979e793e248c8f89a63d9c88167b7d69998be9f23707f567c4f76`
and `ev_4c398aef4eca21d327b89cdf7375cbfb1c6ba3354375d36720c682eb92992d03`.
Analyzed common-bundle thin x86_64 SHA-256:
`52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d`.
Static evidence establishes layout; no live timing behavior has been observed.

## Legacy private-ring snapshots

`include/legacy_ring_snapshot.hpp` can inspect an **offline copy** of a recovered
4096-byte legacy ring. It validates little-endian read/write/capacity fields at
offsets 0/4/8, capacity 1021, and indices within capacity before copying queued
opaque 32-bit words from offset 12. One slot is reserved, leaving 1020 usable.
This structure comes from this pass's coordinating REA analysis of the MIDI
driver, thin x86_64 SHA-256
`52a173fd23b2345b4d32f9752144c2b66fd33f55a787564c4063ae5c7851534f`.
Retained evidence (REA/Ghidra static analysis, not runtime observations):

- [Init 0x8600](../../analysis/decompiled-pass3/midi-0x8600.c),
  `ev_cd1bfe7709aabaf931c57a1bf097ef1428044ea21483466420b03734fb87b54d`.
- [Write 0x80d0](../../analysis/decompiled-pass3/midi-0x80d0.c),
  `ev_c35a3e4d07675e39f372d545fd0db92d3ebaca0af4875dfc87a456196af87195`.
- [Next 0x8270](../../analysis/decompiled-pass3/midi-0x8270.c),
  `ev_5f08fd20e20374f02aab05f0d67f6ac8ff12c6cae9723ddbebf07edffd4fe219`.
- [Full 0x82b0](../../analysis/decompiled-pass3/midi-0x82b0.c),
  `ev_5f21dea558fedccecf76f5cd6d848c96f9235acb0858249beb187dbeefd2577c`.
- [AllocateRead 0x6720](../../analysis/decompiled-pass3/midi-0x6720.c),
  `ev_22a80652cdb999ae5f08f7cdbbf4b0ba404fe58a2226d28dbeb149ba34c6b770`.
- [AllocateWrite 0x6910](../../analysis/decompiled-pass3/midi-0x6910.c),
  `ev_f3fc285b240a1edd0ccca7099c6dcbaa0447d98a187741dde66ffb34cb8667df`.
- [Read 0x81c0](../../analysis/decompiled-pass3/midi-0x81c0.c),
  `ev_b58a97f0a965c7584341405a540fb9e7bcb4891ac3cfb0ef052adaf6d4195270`.
- [Empty 0x8190](../../analysis/decompiled-pass3/midi-0x8190.c),
  `ev_0b5409a08cac86a7d16f26d0357d2cfea69be1bcc052cada31ee8a072959560a`.

The read implementation corroborates that read_index points to the next unread
word, and the empty check compares read_index with write_index. The API
does not map live memory, change cursors, reproduce semaphores, infer timestamps,
or interpret word payloads. Callers must supply an immutable, coherent snapshot
and nonoverlapping output storage. A raced live memory copy is not proven coherent
by header validation.

## Offline verification

From the repository root, run `sh native/midi-core/build_and_test.sh`.
The script builds ARM64 on macOS, then repeats under AddressSanitizer and
UndefinedBehaviorSanitizer. Sanitizer failures fail the script. It uses Apple's
`/usr/bin/python3` by default on macOS; `PYTHON` and `CXX` can override executables.

Passing coverage includes exact CC/poly-pressure/SysEx bytes, rejected builder
inputs without partial writes, running status, system common messages, real-time
interleaving, timestamps, malformed/truncated streams, exact SysEx limits,
overflow recovery, and deterministic stress over 1,024,000 random bytes. The
stress check compares whole-buffer parsing with arbitrary split boundaries. This
validates this module's invariants, not hardware interoperability or real-time
timing under host load.

USB codec tests additionally cover USB-IF golden packets, all channel statuses on
all 16 cables, every possible CIN F byte on every cable, multi-cable transfers,
strict rejection without partial output, and 2,047 complete SysEx roundtrips
(lengths 2 through 2,048) through packet encoding, decoding, and stream assembly.
