## Problem and change

Describe the user-visible problem and resulting behavior. Link the issue/test report.

## Validation

- Source commit and relevant checks run:
- Hardware/firmware, Mac/chip, macOS, DAW, USB path, rate/block (if applicable):
- Pass/fail results and untested behavior:
- UI changes: before/after captures and scales checked:
- Hardware changes: original-state snapshot and restoration outcome:

## Review checklist

- [ ] This targets `main` from a separate branch; scope is focused.
- [ ] Documentation and meaningful tests match the change.
- [ ] No vendor payloads, generated tables, credentials or private session data are included.
- [ ] No claim of hardware support is based solely on mocks, compilation or screenshots.
- [ ] Any protocol/allowlist change is identified explicitly, with recovery implications.

Wait for passing CI and independent maintainer review. Do not bypass protection
or push to `main`; see CONTRIBUTING.md and docs/MAINTAINING.md.
