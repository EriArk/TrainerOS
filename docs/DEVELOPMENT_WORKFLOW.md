# Engineering workflow

TrainerOS uses a complete delivery loop: inspect → implement → review → verify → commit → push → verify the remote commit. The user has authorized this routine for project work. A local commit alone is not a delivered increment. Since the user's 2026-09-13 instruction, GitHub Actions is not a delivery gate; verification runs on the Windows host, Linux build server and Flip as appropriate.

## Before changing code

### Whole increments and avoiding loops

Use [ROADMAP](ROADMAP.md#active-execution-queue--2026-10-04) for order and
[Current tasks](CURRENT_TASKS.md) for the current outcome and remaining acceptance.
Do not choose a new task from an old checkpoint's "next" sentence.

1. State the task ID, whole user-visible outcome and reasoning level. If the
   recommended level changes, wait for the owner's continuation before starting.
2. Read existing evidence once. Identify what is actually missing, distinguishing
   implementation from unverified behavior and unavailable external conditions.
3. Complete the announced functionality and its direct controller journey, then
   run the necessary bounded checks. A test is part of the feature, not the next
   feature. Do not add speculative code merely to make a verification look larger.
4. Repeat a passed check only for an affected change, observed failure or named
   unresolved risk. Record that reason. Avoid general re-audits of settled choices.
5. Deliver relevant changes to both available handhelds, review actual screenshots
   for visual changes, update task/evidence records, commit and push. Docs-only
   planning needs document/link/diff verification, not builds or device deployment.
6. Report the result against the announced scope. Unmet acceptance stays open;
   name its exact dependency. Do not relabel a partial checkpoint as a whole block,
   silently split the promised work, or jump the queue to hide a blocker.

If the remaining work requires additional clients, different internet connections
or owner listening, say so once and retain it in the task register. Continue only
independent authorized work in the accepted scope; when none remains, stop rather
than manufacturing another test-only pass. A standalone audit/planning pass is
appropriate when the owner explicitly requested it.

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

Run `link_peer` tests in an isolated network namespace when TrainerOS is live on
the build host. The test protocol listens on port 47845; `--network=host` containers
share the handheld's real listener and are not a safe protocol-test environment.
For Podman, use a separate test container with `--network=none`, loopback enabled,
the build's Qt dependencies and the existing source/build mounts. Build compilation
may retain its original networking. Do not stop a user's live session or weaken
pairing assertions merely to resolve a test-port collision.

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

Owner request, 2026-10-06: GitHub Actions is disabled at repository level and
the obsolete workflow has been removed. Do not restore automatic or manual
Actions runs without an owner request. Local/native and appropriate device checks
remain required; documentation-only work does not require handheld deployment.

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

**Owner delivery preference, 2026-09-28:** update both Flip 2 and Odin 2 in each
device delivery increment when they are reachable. Check SSH and the running
session first; preserve each device's own profiles, library, emulator settings,
saves and boot preference. Include required integration helpers, not only the
binary, and verify the live version separately on each device. Never copy one
device's personal state onto the other to make versions match. If a device is
asleep/unreachable or has an active game, report its pending update explicitly;
do not present a successful Flip deployment as proof of Odin delivery.

Before reporting completion, check the working tree and upstream again. Give a concise result: what changed, the pushed commit/link, which relevant checks passed, and unresolved limitations. Explicitly identify any remaining task edits; a clean tree must not be achieved by discarding them.

Keep these evidence levels separate:

| Evidence | What it establishes |
| --- | --- |
| Windows build/tests and rendered SDL scenarios | Behavior on the development host and exercised fixtures |
| Linux build server with the current source | Build/test behavior on that server's Linux/Qt/SDL environment |
| Flip 2 running the actual ArmadaOS build | Physical mapping, display readability/performance, emulator environment and observed device/session behavior |

Linux server checks do not close the ARM64/handheld gate. Follow `FIRST_DEVICE_RUN.md` and `DEVICE_DIAGNOSTICS.md` before real ArmadaOS integration or changing the normal session. If progress in the agreed sequence needs the handheld, state the exact dependency and the prepared next step.

## Adapter research knowledge (#91)

Before any game/save research or adapter edit, consult [the adapter knowledge base](adapters/README.md). Register exact targets and mark new capabilities researching before implementation; append meaningful sourced findings during work. Update per-capability evidence and registry with the code commit. A stale record means the increment is not done. Keep private data outside Git; #89 remains a bounded interface audit.

Owner clarification, 2026-09-27: the permanent knowledge base also keeps a reusable copy of the actual adapter configured per exact game/revision. Refresh `tools/export-game-adapters.py` with adapter/data/profile edits and pass `tools/check-adapter-knowledge.py` before delivery. Include dependencies, tables, source attribution, a build example and host-protection boundaries; no private game content.
