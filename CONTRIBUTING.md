# Contributing to Virus TI Rework

Hardware reports, reproducible bugs, documentation fixes and code contributions
are welcome. You do not need a second synth or programming experience to help.
Keep discussion respectful and describe observations precisely.

## Test a setup

Read the [testing guide](docs/HARDWARE_TESTING.md), then open a **Hardware / macOS
compatibility report** using the issue form. Successful, failed and partial
results all help. Report the exact Mac/chip, macOS version and build, synth model
and firmware, DAW/version, USB path, sample rate/block size and source commit.
Do not combine results from different configurations into one unexplained pass.

The [compatibility matrix](docs/COMPATIBILITY.md) distinguishes recorded project
tests from community reports and untested configurations. Maintainers update it
through PRs after reviewing the linked evidence. One report is not certification
of an entire Mac generation, synth family or macOS release.

## Code and documentation: branch → PR → review

1. Search existing issues/PRs. For a substantial hardware, protocol or design
   change, open an issue describing the problem before a large implementation.
2. Fork the repository when available, then create a focused branch from current
   `main` (for example `fix/browser-navigation`). Invited collaborators also use
   branches. Never push directly to `main`.
3. Keep the patch focused. Explain the behavior before and after the change and
   record any assumptions. Update relevant documentation and meaningful tests.
4. Run the checks below, commit, push your branch and open a PR targeting `main`.
   Use a draft PR if work is incomplete. Complete the supplied PR template.
5. Wait for an independent maintainer review and passing checks. Address review
   threads and update your branch when `main` changes. New commits invalidate
   earlier approval under the intended protection policy.
6. A maintainer squash-merges the reviewed PR; the branch can then be deleted.
   Do not merge your own unreviewed PR, use a bypass or force-push `main`.

Contributors do not receive repository write access merely by opening an issue
or PR. CODEOWNERS requests review; it does not grant merge permissions. Actual
enforcement depends on GitHub branch protection: see [maintainer setup](docs/MAINTAINING.md).

## Checks

Source-only checks, requiring no Access installer or connected synth:

```sh
python3 tools/check_repository.py
bash -n setup.sh tools/install_local.sh native/vst3/build_and_test.sh native/vst3/bootstrap_sdk.sh
python3 -m compileall -q tools native/vst3/generate_catalog.py
sh native/transport-core/build_and_test.sh
```

On Apple silicon, also run `sh native/clock-sync/build_and_test.sh` for changes
to timing code. The GitHub `offline-core` job checks tracked-file boundaries,
shell/Python syntax and the portable transport tests with sanitizers. It does
not prove that the macOS plug-in compiles or that a connected synth works.

For editor, processor, service or packaging changes, build locally using `bash tools/build_beta.sh`. No Access package is required.
This builds AU/VST3 and Setup, runs the project suites, VST3 validator, packaging audit and installation transaction tests. macOS CI also runs `auval`. Optional original-art importer tests require a locally owned installer. Include
the outcome in the PR. UI changes should include before/after captures and
checks at more than one scale. Do not label synthetic meter fixtures as live audio.

Hardware-affecting changes need bounded device checks and restoration evidence
for the relevant path. If you lack the device, say so and request testing rather
than claiming a pass. Firmware transfer and stored-bank writing are outside this
project's current scope; do not extend the allowlist casually.

## What belongs in Git

New implementation, reproducible tools, documentation and permitted test data
belong here. Use the MIT licence for your new contributions; retain provenance
and third-party notices. Do not submit material you cannot contribute under its
applicable terms.

Do not commit the Access installer, extracted XML/artwork/tables, firmware,
factory banks, raw decompiler output, personal patch dumps, recordings/projects,
session directories, SDK checkouts, built plug-ins, signing keys or credentials.
Generated files stay local. `tools/check_repository.py` checks the Git index,
so stage your intended files before using it to inspect a proposed commit.

Share short redacted log excerpts and counter summaries. Remove usernames,
serial numbers, device identifiers, private paths and personal patch names.
Review attachments yourself; a log is not automatically safe to publish.

For a suspected exploitable security issue, do not post an exploit or sensitive
data in a public issue. Use GitHub's private vulnerability-reporting option if
enabled; otherwise request a private contact channel without disclosing details.
