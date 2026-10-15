# Hardware and macOS testing guide

The goal is to learn which **exact combinations** work. Do not assume support
from a successful build, an editor screenshot or a different TI model.

## Record your configuration

Capture these before starting, and include them in the hardware-report issue:

| Field | What to record |
| --- | --- |
| Source | `git rev-parse HEAD`, local changes, build date and setup result |
| Mac | Model/year and exact Apple chip, including variant; memory if relevant |
| macOS | Version and build from `sw_vers`; state whether a beta |
| Synth | TI Desktop / TI Keyboard / TI Polar / TI2 variant / Snow; exact firmware |
| Host | DAW and version, native arm64 or Rosetta, plug-in isolation mode if known |
| USB | Direct connection or hub/dock; model, adapter/cable type and other devices |
| Audio | DAW output interface, host rate, block size and enabled VST3 output buses |
| Scope | Steps performed, duration and pass/fail/not-tested for each stage |

Do not include machine or synth serial numbers. Prefer already available setups;
there is no need to buy hardware or change a working OS to contribute. macOS 12+
is a build target, not a statement that every intervening release works. Reports
on older supported macOS releases and new point updates are both useful.

## Preparation and stop conditions

Use a disposable DAW project and preserve important sounds using your existing
backup method. Start with low monitoring volume. Avoid changing the synth's
controls while a snapshot/save/restore is in progress. Keep the synth connected
and powered until recovery finishes.

For an untested model, begin with build/scan and read-only discovery. The service
currently assumes the Desktop's sixteen-part topology; Snow and other variants
need maintainer confirmation of their descriptors, part count and protocol before
attempting connection or recall. Mark playback stages not tested where unsuitable.

### Read-only diagnostics for Snow and other unverified models

The [model audit](device-model-audit.md) lists discovery aliases and the remaining
transport restrictions. Snow connection is deliberately disabled in the detection
fix; do not bypass it or repeatedly retry Connect in an older build.

No Terminal or driver changes are needed to collect these details:

1. Record the Mac model/chip, macOS version/build, DAW point version, native/Rosetta
   mode and Rework release version. Record the synth firmware already displayed
   by the synth; do not update it for this test.
2. Open **System Information → USB**, select only the Virus device, and record
   its displayed product/manufacturer name, Vendor ID, Product ID and Version
   (USB device revision). In **Audio**, record its name, channel counts and rate.
   Do not infer TI versus TI2 or Snow from Product ID alone.
3. In **Audio MIDI Setup → Window → Show MIDI Studio**, report the exact Virus
   device and port names, especially `USB Plugin I/O` and `USB External I/O`.
   The Audio Devices window is a separate view. Seeing USB audio does not prove
   that the MIDI driver and its ports are available.
4. State which MIDI driver is installed and its version, or say unknown/none.
   Our current transport needs compatible CoreMIDI ports; it does not bundle a
   native replacement MIDI driver. Report direct USB versus the named hub/dock.

Share only the selected device details or cropped screenshots. Redact serial
numbers, location IDs, usernames and private paths; do not attach a full system
report, patch dumps or personal projects.

Stop if a snapshot, checksum, identity or restore error appears. Save a redacted
error summary and report it; do not bypass validation or repeatedly retry writes.
No firmware update, stored-bank write, SIP/Gatekeeper disablement, unexpected USB
unplug or forced DAW crash is required for this community test.

## Staged checklist

Use **pass**, **fail**, or **not tested** for every row. Add elapsed times and
observations; a blank cell must not be interpreted as a pass.

| Stage | Procedure and useful evidence |
| --- | --- |
| Build | Run setup without `--install` first; record the six-suite/validator results and any compiler/dependency failure. |
| Scan/editor | Install the local bundle, restart the host, create a fresh instrument track; check scan, all pages, text/knob alignment, theme and scale. Hardware need not be connected. |
| Discovery | Record whether macOS sees the synth. If using `native/virus-probe`, follow its README and share a redacted summary, not an entire registry dump. Presence alone does not prove compatibility. |
| Connect | On a maintainer-supported topology, start in Single mode and press Connect TI once. Wait for initialization; record status and any snapshot failure. |
| Basic notes | Send a short MIDI clip on channel 1. Record note on/off behavior, audio return, stuck notes and subjective clicks. A meter moving alone does not prove clean sound. |
| USB returns | Test each available stereo bus independently using normal DAW routing; distinguish unsupported/unexposed buses from a failed test. Note the DAW's physical output device. |
| Parts | Test only the confirmed part count. Record which channels were auditioned and whether other parts changed unexpectedly. Sixteen parts on one TI Desktop does not imply sixteen on Snow. |
| Preset/control | Recall into one selected part and check another part remains unchanged. Exercise a documented supported control. Record local-only controls separately. |
| Real-time playback | Run a short session, then a longer session if stable. Report duration, glitches and start/end capture/FIFO counter deltas; zero new errors matters more than a lifetime total. |
| Project recall | Save a dedicated project while connected; allow the snapshot to finish. Close/reopen normally and check sound, routing and reconnect behavior. Report save timeout separately. |
| Manual restore | Disconnect and wait; record the restoration result and whether the original sound/routing returns. |
| Normal host exit | In a separate disposable session, quit normally while connected. Keep hardware powered, check `ti_control.py restorations`, and report success/error plus time. Do not force-quit as a routine test. |

If practical, repeat a stable basic playback test at a second host sample rate
or buffer size. Change one variable at a time and report the resulting combination
separately. The device's stream stays at 48 kHz; host-rate tests exercise resampling.
Offline export is not a supported way to render the hardware.

## Reporting

Use the **Hardware / macOS compatibility report** issue form even if everything
you tested passed. Include the table above with explicit outcomes, short redacted
errors, and whether the run used this commit unmodified. For a regression, identify
the last known working commit if available. Link a separate bug issue for a focused
failure. Do not upload firmware, patch dumps, personal projects or entire session
folders. The [matrix](COMPATIBILITY.md) only gains a reported result once its
configuration, tested scope and evidence link are available.
