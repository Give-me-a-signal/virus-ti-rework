# Native AU and VST3 beta

Build from the repository root with `bash tools/build_beta.sh`. No Access installer
or extracted resources are needed. The tool fetches pinned SDKs, builds native arm64
AU/VST3 and Setup, runs tests and the VST3 validator, audits runtime dependencies and
packages an ad-hoc signed ZIP. GitHub CI also checks installed AU discovery with auval.

The Audio Unit embeds a complete VST3 and restoration helper, so it installs on its
own. Both formats use the same engine, state readers, sixteen MIDI channels, three
stereo returns and exclusive device lease. See `docs/BINARY_ARCHITECTURE.md` for the
resource boundary and `docs/beta-validation.json` for explicitly scoped results.

Old development class IDs and parameter values are preserved. Original vendor plug-in
IDs/state formats are not supported. Dark/Polar are self-contained; the optional
original appearance is imported with the native picker and stored outside bundles.
