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

**The protection rule is prepared but NOT active.** GitHub rejected both reading
and applying private-repository branch protection with HTTP 403:
“Upgrade to GitHub Pro or make this repository public to enable this feature.”
Repository visibility remains private. CODEOWNERS, templates and this document
do not substitute for server-side protection; users with existing write/admin
access can still push directly until activation succeeds.

Applied and read back successfully: squash-only merges, automatic branch cleanup
and disabled auto-merge. The status check is bound to GitHub Actions app ID 15368
and its existing `offline-core` job, observed on the initial passing workflow.

The account owner can enable private-repository protection by choosing a suitable
GitHub plan, or enable it after an explicitly approved public release. Do not
change visibility or billing as a side effect of running repository setup.
[GitHub's protected-branch availability](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
describes the plan requirement.

## Reviewer setup

The initial code owner is `@Give-me-a-signal`. A PR author cannot approve their
own PR. Before activating strict protection for owner-authored work, the owner
must appoint a second trusted reviewer with appropriate repository access and
add them to CODEOWNERS. No new collaborators are invited by this change. Keep
the independent review requirement rather than silently introducing an admin
bypass when a second reviewer is unavailable.

## Apply and verify the prepared rule

Once the plan/visibility supports it and reviewer access is ready, an administrator
can apply the reviewed configuration from the repository root:

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

After public release, require approval for workflows from all external contributors
in Settings → Actions → General. That public-fork approval setting currently returns
HTTP 422 on this private repository. Keep workflow tokens read-only; never run
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
