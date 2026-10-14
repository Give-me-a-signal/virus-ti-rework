# USB descriptor acquisition and offline parser

This is native ARM64 groundwork for discovering the real Virus USB audio topology.
It is **not an audio driver** and does not stream audio. Hardware acquisition is
compiled but has not been tested with a connected Virus. All test fixtures are
invented USB2 configurations, not observations about Virus endpoints or formats.

Build and test from the project root on macOS with Command Line Tools and Python 3:

```sh
sh native/usb-descriptors/build_and_test.sh
```

The build runs C++ tests normally and with AddressSanitizer/UndefinedBehaviorSanitizer,
compiles the IOUSBHost CLI, and runs offline CLI integration tests. It never opens
USB hardware. RTK is used if installed, otherwise commands run directly.

## CLI modes

```sh
# Default: print help only.
native/usb-descriptors/build/virus-usb-descriptors

# Registry-only enumeration of VID 133e, PID 0815/0816; does not open devices.
native/usb-descriptors/build/virus-usb-descriptors --discover

# Parse one binary configuration descriptor including its subordinate descriptors.
native/usb-descriptors/build/virus-usb-descriptors --file CONFIG.bin --speed full

# Later, when ready for a hardware session:
native/usb-descriptors/build/virus-usb-descriptors --inspect-connected > descriptors.json
```

`--inspect-connected` is deliberately separate from the existing read-only
`virus-probe`. IOUSBHost initialization **claims exclusive device ownership**, even
with `IOUSBHostObjectInitOptionsNone`. Holding the object can prevent other drivers
from changing device state. Descriptor retrieval can use cached bytes or issue
standard GET_DESCRIPTOR requests. Therefore this is not a passive registry probe.
The code uses neither capture nor seize, changes no configuration or alternate
setting, and calls no reset, sample-frequency request, firmware request, interface
open, pipe I/O, or streaming operation. It releases the session with `destroy`.
It visits every matched device; attach only the intended Virus for the first test.

macOS may deny the session because of permissions or an existing owner. Errors
include the API's domain/code/description; there is no automatic escalation,
entitlement change, driver unload, capture, or retry with stronger options. The
unsigned development CLI's real-device access has not been validated. Missing
hardware returns 3, not a compatibility verdict. Do not infer driver readiness
from a successful build or registry snapshot.

Output is JSON. Each successfully inspected device includes the fixed 18-byte
device descriptor as `device_descriptor.raw_hex` and decoded `bcdUSB`, `bcdDevice`,
device class/subclass/protocol, endpoint-zero packet size, VID/PID, and configuration
count. The raw fixed descriptor contains string **indices**, including
`iSerialNumber`; it contains no serial-number string or other string content.
This retains a baseline for firmware/release and device-mode comparisons.
Registry queries match exact VID/PID pairs before identity properties are read;
failed/changed identity properties on a returned match produce a query error,
not a successful absence result. Registry entry-ID errors are also reported.
Device/configuration descriptor calls use actual lengths reported
by the API, and configurations are validated before interpretation. No string
descriptors or serial numbers are queried. Configuration JSON preserves `raw_hex`
for subsequent offline parsing and all descriptor bytes in original order. Extract
a selected configuration's `raw_hex` with `bytes.fromhex(...)` to make CONFIG.bin.
An acquisition failure or malformed configuration returns 4; input/file errors
return 2. Hardware output intentionally leaves speed interpretation unknown; it
does not guess from an unverified registry speed number.

## Parser contracts and limits

The C++17 parser accepts exactly one configuration blob, requires exact
`wTotalLength`, checks every descriptor's bounds and minimum header length,
validates standard configuration/interface/endpoint lengths, and rejects duplicate
alternates, duplicate endpoint addresses per alternate, orphan endpoints, endpoint
zero/reserved address bits, missing alternate zero, and interface/endpoint count
mismatches. IN/OUT endpoints sharing a number are distinct. Exceptions include the
offending byte offset. Unknown and class-specific descriptors survive verbatim;
their internal structures are not yet decoded or semantically validated.

Each alternate retains class/subclass/protocol and its own endpoints. Endpoint
attributes expose transfer type and, for isochronous endpoints, synchronization and
usage type; the nine-byte audio endpoint extensions remain in raw bytes. Raw
`wMaxPacketSize` and `bInterval` are always retained. USB2 capacity and interval
interpretation is explicit about speed: high-speed periodic endpoints use up to
three transactions and 125 microsecond units; full-speed isochronous endpoints use
the interval exponent in 1 ms units. Full/low-speed interrupt intervals are linear
milliseconds. Invalid or unsupported capacity/interval interpretations return no
value. USB3 companion descriptors are preserved but not interpreted. Capacity is a
descriptor ceiling, not an audio channel count, transfer schedule or measured rate.

No active alternate is inferred from a configuration listing. USB Audio class
format/rate descriptors, feedback endpoints, vendor-specific mode transitions,
isochronous scheduling, ring buffers, clock drift, and CoreAudio exposure still
need implementation and hardware evidence.

Validation on 2026-10-11: ARM64 normal and ASan/UBSan tests passed, including every
truncation of a synthetic configuration, malformed structure/counts, explicit
interval/capacity vectors, and 20,000 deterministic byte mutations. Offline CLI
tests passed. Registry discovery returned no matching hardware and no query error.
`--inspect-connected` was not run.

## API and format sources

- Installed macOS SDK: `IOUSBHost.framework/Headers/IOUSBHostObject.h` documents
  exclusive ownership, `destroy`, descriptor cache/GET_DESCRIPTOR behavior, and
  the actual-length output of `descriptorWithType:length:index:languageID:error:`.
- [Apple IOUSBHostDevice](https://developer.apple.com/documentation/iousbhost/iousbhostdevice?language=objc)
  and [IOUSBHostObject](https://developer.apple.com/documentation/iousbhost/iousbhostobject)
  document the user-space device object and descriptor retrieval.
- [USB-IF USB 2.0 specification](https://www.usb.org/document-library/usb-20-specification),
  chapter 9 descriptor layouts and endpoint interval/packet fields. These generic
  USB interpretations are distinct from reverse-engineered Virus behavior.
