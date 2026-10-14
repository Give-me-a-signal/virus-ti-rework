# Repository governance and activation

## Policy

All changes, including maintainer documentation changes, go through a branch and
pull request. Contributors use forks or explicitly granted collaborator access;
opening an issue/PR does not confer write access. Do not use direct pushes to
`main` as the normal workflow.

The prepared `main` protection requires:

- A PR with one independent approving review and code-owner approval.
- Dismissal of stale approvals after new commits, plus approval of the latest
  reviewable push by someone other than its author.
- The GitHub Actions `offline-core` check passing on an up-to-date branch.
- All review conversations resolved and a linear history.
- Enforcement for administrators as well as other writers, with force pushes
  and deletion disabled. No review bypass list is configured.

Only squash merging is enabled. Merged branches are automatically deleted;
automatic merging is disabled. Review source-only CI separately from local
macOS/editor/hardware evidence: CI cannot establish that the synth works.

## Activation status — 14 October 2026

**The repository is public and the `main` protection rule is active.** On
14 October 2026, GitHub accepted the prepared configuration after the owner
requested public visibility. The API readback confirmed required PR/code-owner
review, one independent approval, stale-review dismissal, latest-push approval,
strict `offline-core` checks, administrator enforcement, resolved conversations,
linear history, and disabled force pushes/deletion.

Squash-only merging, automatic merged-branch cleanup and disabled auto-merge are
also enabled. The required check is bound to GitHub Actions app ID 15368 and its
`offline-core` job. Workflow runs from all external PR contributors require
maintainer approval; this was applied and read back after publication.

Previously, GitHub returned HTTP 403 for protection while this repository was
private on the account's plan. That limitation no longer applies to this public
repository. CODEOWNERS and policy files alone are not enforcement; the active
server-side settings are what protect `main`.

## Reviewer setup

The initial code owner is `@Give-me-a-signal`. A PR author cannot approve their
own PR. To merge owner-authored work under the active protection, the owner
must appoint a second trusted reviewer with appropriate repository access and
add them to CODEOWNERS. No new collaborators are invited by this change. Keep
the independent review requirement rather than silently introducing an admin
bypass when a second reviewer is unavailable.

## Apply and verify the prepared rule

To reapply a reviewed configuration, an administrator can run from the repository
root:

```sh
gh api --method PUT repos/Give-me-a-signal/virus-ti-rework/branches/main/protection \
  --input .github/main-protection.json
gh api repos/Give-me-a-signal/virus-ti-rework/branches/main/protection
```

Read back the rule and verify all required fields, especially `enforce_admins`,
the check's context/app ID, review count, code-owner review, stale review dismissal,
latest-push approval, conversation resolution, and disabled deletion/force push.
Update the activation status in this file through a PR only after verification.
Changing `main-protection.json` alone does not update GitHub settings.

Approval for workflows from all external contributors is enabled in Settings →
Actions → General. Keep workflow tokens read-only; never run
untrusted PR code with secrets via `pull_request_target`. Review workflow changes
and resource-boundary changes carefully. The existing workflow uses `pull_request`,
read-only contents permission and a pinned checkout action.

## Test-report triage

Confirm configuration and source commit, distinguish reports from independently
reproduced results, and ask for missing steps without requesting private dumps.
Add matrix rows only by PR with a report link and bounded pass/fail/not-tested
scope. Do not turn a single successful report into a claim of universal support.

## Release discipline

Keep vendor resources, generated tables, raw evidence and personal sessions out of
Git and public artifacts. Public visibility does not itself authorize redistributing
the local ZIP. Follow THIRD_PARTY_NOTICES.md and the README release plan. Before
publishing a binary, document permitted resources, signing/notarization, tested
configurations, known limits and reproducible checksums.
