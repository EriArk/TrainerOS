# Engineering workflow

TrainerOS uses a complete delivery loop: inspect → implement → review → verify → commit → push → verify the remote commit. The user has authorized this routine for project work. A local commit alone is not a delivered increment. Since the user's 2026-09-13 instruction, GitHub Actions is not a delivery gate; verification runs on the Windows host, Linux build server and Flip as appropriate.

## Before changing code

Read the repository contract and the relevant product/architecture/roadmap documents. Inspect `git status --short`, the active branch, upstream and recent commits. Understand existing changes before staging or editing them. Fetch remote state before integration/push; do not overwrite someone else's changes to make the tree look clean.

Choose one bounded increment with a visible acceptance result. Follow foundations → modules → verified adapters; do not jump to save parsing, external achievement integration or session replacement to avoid a missing device prerequisite. Record meaningful decisions in the same change.

Keep UI, domain/repositories, emulator adapters and platform services separated. Preserve controller invariants, honest unknown data and external save ownership. Tests and mock providers are substitutes for specific development dependencies, not proof that the physical device or a real emulator works.

## Verify the change

Use the established build directory and toolchain in `DEVELOPMENT.md`. Typical native verification:

```sh
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure
git diff --check
```

Scale checks to risk. An affected module may need focused tests; changes to shared input, persistence, startup/exit or launch/return normally need the broader suite. Exercise real child processes for lifecycle changes and real database reopen/migration for persistence. Do not replace these with tests that only repeat the implementation.

For QML, use the rendered application with SDL virtual-controller events, inspect actual focus/Back/global navigation, and visually check the changed views at handheld landscape dimensions. Review empty/loading/error states and bounded scrolling. Keep test output/captures outside commits. Run a non-testing build when testing guards, packaging or composition boundaries change.

Documentation-only and reversible low-impact changes should get relevant content/link/diff checks. Do not rerun unrelated tests solely to make a report sound thorough. If checks fail, diagnose and fix the cause; do not weaken assertions or suppress warnings to obtain a green result.

## Commit reviewed work

Stage explicit paths, then inspect both the staged content and file inventory. `git diff` without `--cached` does not review newly staged files. Generated reports/builds, machine-specific helpers, downloaded research, credentials, ROMs, saves, BIOS/keys and unauthorized artwork are excluded.

```sh
git add -- path/to/reviewed/file path/to/another/file
git diff --cached --stat
git diff --cached
git diff --cached --check
git commit -m "Describe the concrete change"
```

Keep messages concise and commits coherent. Include relevant docs and acceptance changes. Commit at the end of each validated increment; do not accumulate finished stages between user turns. Existing uncommitted work must be understood and deliberately included or left intact, never silently discarded or hidden behind an ignore rule.

On the Windows checkout, use the established line-ending policy. Shell scripts are LF via `.gitattributes`. If necessary, use command-local `-c core.safecrlf=false` for Git's normalization/check commands; do not change the entire checkout's autocrlf setting or convert unrelated files merely to silence warnings.

## Push and verify GitHub

Fetch the intended remote and compare its branch to the local branch. Push to the active intended upstream after the increment passes local checks. Use branch protections and PR requirements if present; no routine force-push, shared-history rewrite or protection bypass is authorized.

For the current `main` / `origin/main` arrangement:

```sh
git fetch origin
git rev-list --left-right --count origin/main...HEAD
git push origin main
git rev-parse HEAD
git ls-remote origin refs/heads/main
git status --short --branch
```

Resolve divergence without discarding remote work. If the remote destination or ownership of conflicting edits is ambiguous, establish it before changing shared history. A rejected push is not a successful delivery.

Do not poll or wait for GitHub Actions, or spend project time on its billing. This does not waive native builds, relevant tests, controller/rendered checks or device verification. Record the source revision and toolchain for those checks, and verify the pushed SHA independently with Git. Existing workflow files are not evidence that Actions ran or passed.

## Completion report and device boundary

Before reporting completion, check the working tree and upstream again. Give a concise result: what changed, the pushed commit/link, which relevant checks passed, and unresolved limitations. Explicitly identify any remaining task edits; a clean tree must not be achieved by discarding them.

Keep these evidence levels separate:

| Evidence | What it establishes |
| --- | --- |
| Windows build/tests and rendered SDL scenarios | Behavior on the development host and exercised fixtures |
| Linux build server with the current source | Build/test behavior on that server's Linux/Qt/SDL environment |
| Flip 2 running the actual ArmadaOS build | Physical mapping, display readability/performance, emulator environment and observed device/session behavior |

Linux server checks do not close the ARM64/handheld gate. Follow `FIRST_DEVICE_RUN.md` and `DEVICE_DIAGNOSTICS.md` before real ArmadaOS integration or changing the normal session. If progress in the agreed sequence needs the handheld, state the exact dependency and the prepared next step.
