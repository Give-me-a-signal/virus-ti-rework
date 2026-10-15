# Resource-independent beta architecture

The public engine preserves the development VST3 IDs, 2,672 parameter records,
plain/normalized scaling and version 1–3 state readers. `parameter_registry.h`
is the project's compact compatibility schema: identifiers and numeric MIDI
interoperability facts, not a copied XML tree or vendor value-list collection.
The legacy Part 3/Part 4 Filter Frequency assignment is retained intentionally;
changing it requires an explicit state migration.

Dark and Polar use an authored control grid and system fonts. Analytic waveform
illustrations and schematic filter/envelope geometry replace extracted display
tables. Disabled controls are roadmap displays, not verified synth edits. The
graphs describe controls; they do not emulate the hardware audio engine.

AUv2 wraps the same native VST3 processor/controller. Its component embeds a real
VST3 copy and helper, with no absolute build-directory links. The permanent AU
identity is `aumu / VtRw / GmAS`. The SDK is pinned to AudioUnitSDK 1.4.0 commit
`bd98b31feff57a15989fcfab4cd86dc63382b1ac` (Apache-2.0); Steinberg remains pinned
by `native/vst3/sdk-lock.json` (MIT). New code remains MIT; reverse engineering
informed protocol behavior, so this is not a clean-room claim.

The optional native importer checks the exact Access package SHA-256, invokes
macOS `pkgutil --expand-full` and copies only PNGs and three editor XML files.
It does not run the Access installer, plug-ins, updater or drivers. A bounded
XML parser builds the original appearance model at runtime. Validated resources
are atomically activated in per-user Application Support, outside signed bundles.
They retain their original ownership and must not be added to release archives.

The setup app verifies file hashes and ad-hoc signatures before installation,
stages both formats, backs up existing project bundles and rolls back replacement
on failure. User sounds, optional artwork and restoration evidence are preserved
on uninstall. Checksums detect damaged packages; without Developer ID signing
they do not authenticate the publisher.

Release CI starts without Access resources. Packaging rejects vendor payload
extensions, unexpected symlinks and embedded checkout paths. Tags publish only
after the macOS build, tests and validators pass and the tagged commit is on
reviewed `main`. Repository review protection is not bypassed for release work.
