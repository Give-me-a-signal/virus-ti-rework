# Virus read-only discovery probe

A native Apple silicon command-line utility for taking a connection baseline before
developing a replacement Virus TI USB transport. This is a diagnostic utility, not
an audio driver or the reworked Virus Control plug-in.

## Build and run

Requires macOS 12 or later and Apple's Command Line Tools. Run from the repository root:

```sh
rtk proxy sh native/virus-probe/build.sh
rtk proxy native/virus-probe/build/virus-probe > analysis/host-probe.json
```

If `rtk` is unavailable, omit `rtk proxy`. The build script itself uses RTK when
available and falls back to the underlying tool when absent. The executable is
built specifically for arm64. It requires no third-party packages or administrator
access. Build outputs are local and ignored by Git.

After connecting and powering on the Virus, save a separate snapshot:

```sh
rtk proxy native/virus-probe/build/virus-probe --require-device > analysis/connected-probe.json
```

JSON is printed on standard output; argument errors go to standard error.
`--help` describes usage. Exit codes:

| Code | Meaning |
| --- | --- |
| 0 | Snapshot collected; hardware may be absent unless `--require-device` was used. |
| 2 | Unknown argument or JSON serialization failure. |
| 3 | `--require-device` requested and no matching USB device found. |
| 4 | A USB registry enumeration/property query, CoreMIDI client creation, or CoreAudio device enumeration failed. |

Individual descendant and sample-rate queries retain their status codes in JSON.
These partial observations do not change the process exit code. Snapshots are not
atomic: a device connected or disconnected during enumeration may change results.

## Evidence and behavior

The matching IDs come directly from:

`analysis/package-expanded/Driver_package_10.9.pkg/Payload/Library/Extensions/AccessVirusTI.kext/Contents/Info.plist`

Its `IOKitPersonalities` entries declare vendor 4926 (`0x133e`) and products 2069
and 2070 (`0x0815`, `0x0816`). These are the shipped driver's match criteria;
they do not establish which product corresponds to the user's physical model.

The probe:

- Enumerates both `IOUSBHostDevice` and legacy `IOUSBDevice` registry classes,
  deduplicates by registry ID, and reports only matching vendor/product pairs.
- Reads a fixed allowlist of registry properties from matched devices and their
  descendants, including interface and endpoint fields when the registry exposes them.
- Temporarily creates a CoreMIDI client to enumerate source and destination
  endpoints; opens no MIDI ports or connections and sends no messages.
- Reads CoreAudio device names and manufacturers, and nominal/available sample rates
  for devices whose names or manufacturer contain `Virus` or `Access Music`.
- Omits serial properties, CoreAudio UIDs, and MIDI unique IDs. Names and USB
  location IDs remain visible, so snapshots are not guaranteed anonymous.

It does not open or seize a USB interface, issue USB requests, start audio streams,
change sample rates, install drivers, write firmware, or change device configuration.
CoreMIDI/CoreAudio discovery may start Apple's ordinary service processes or load
already-installed components as part of system enumeration.

## Interpretation and limits

`no_matching_registry_device` means only that the matching USB devices were not
observed in the registry snapshot. It does **not** mean that a driver is unavailable,
that the hardware is incompatible, or that the user should install a driver.
`driver_availability` remains `not_determined` in all cases.

Registry properties are not USB descriptor interrogation. Missing endpoint or
interface properties do not establish that the underlying hardware lacks them.
No USB transfer behavior, channel format, streaming performance, clocking, DAW
compatibility, or MIDI protocol support is verified. Renamed MIDI/audio devices can
be missed by name filtering, and published endpoints may exist while hardware is
offline. `offline`, where provided by CoreMIDI, is included in the result.

## Initial host verification

Built with Clang warnings treated as errors and verified by `file` as Mach-O arm64.
The initial saved snapshot in `analysis/host-probe.json` reports macOS 26.2
(25C56), no matching USB device, and no matching MIDI/audio endpoints, with
successful subsystem queries. The user had not connected the hardware.
`--require-device` returned 3 and an unknown option returned 2 as expected.
The connected-device paths still require testing with the real Virus TI.
