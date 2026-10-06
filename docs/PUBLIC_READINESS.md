# Public repository readiness

Audit baseline: `492bff0`, 2026-10-06. This is a repository/documentation pass,
not a new runtime, hardware or legal acceptance. The repository was already
public when reviewed. Existing README screenshots remain at the owner's request.

## Delivered repository cleanup

- GitHub Actions disabled at repository level; obsolete native workflow removed.
  [#7](https://github.com/EriArk/TrainerOS/issues/7) is superseded/not planned,
  not a claimed successful CI implementation. Local verification remains required.
- GitHub About description/topics populated; private vulnerability reporting enabled.
- README now distinguishes source/development tooling, prepared-device evidence,
  pending public-image work and current series collection navigation.
- Current native build/run guide, documentation index, contribution/security
  guidance and issue/PR templates added or corrected.
- Early prototype development instructions archived with a visible historical
  boundary; original Codex bootstrap brief marked historical. Existing acceptance
  and evidence are preserved, not mass-closed as completed.
- Third-party notice inventory added. In the follow-up the owner selected
  **GPL-3.0-or-later** for original TrainerOS code; see [licensing scope](../LICENSING.md).
- Private art-acquisition URLs removed from the current documentation and the
  bodies of #58/#61 without changing their acceptance. Old commits, edit history
  and copies may retain them; no history rewriting was performed.
- Repository-local Codex run/configuration directories excluded from Git. No
  tracked Codex directory was found; the useful bootstrap brief remains documented.
  Global Codex history, private research, credentials and device data are untouched.

No application source, art assets, screenshot files or installed handheld state
changed in this pass. Content/link/diff checks are appropriate; unrelated emulator
and device tests are not repeated. A fresh-machine build was not performed here.

## Open decisions and follow-ups

| Issue | Outcome | Boundary |
| --- | --- | --- |
| [#115](https://github.com/EriArk/TrainerOS/issues/115) | GPL-3.0-or-later adopted; complete distribution notices/source remain open | Existing third-party licenses preserved; verify exact release artifacts |
| [#116](https://github.com/EriArk/TrainerOS/issues/116) | Public asset provenance/defaults and replacement showcase | Keep current README screenshots until a replacement is agreed; no automatic UI redesign/history rewrite |
| [#117](https://github.com/EriArk/TrainerOS/issues/117) | Reproduce clean contributor setup without private machine state | Dependency reconciliation is not a completed clean build; batch proof with relevant build work |
| [#62](https://github.com/EriArk/TrainerOS/issues/62) | Remaining active specification and historical-evidence reconciliation | Updated with this pass; older unresolved acceptance is retained |

The owner accepted **GPL-3.0-or-later** on 2026-10-06. The root LICENSE contains
the unmodified license text and LICENSING.md provides the explicit version-3-or-later
grant and exclusions. README, contribution guidance and reusable source snapshots
carry the decision. The CMake Licensing component installs the project notices.
Declared direct-library licenses were reviewed against the source dependencies;
full binary/image corresponding-source and notice verification remains open in #115.
The initial cleanup above changed no runtime code; this license follow-up adds
only licensing documents and a CMake notice-install rule. Verification: native
CMake configuration succeeded; `cmake --install ... --component Licensing` into
an isolated build directory installed all three root notice files byte-for-byte.
Both reusable license copies match the root's upstream hash; the existing adapter
snapshot check passed. No application rebuild, device deployment or fresh-machine
reproduction is claimed by this metadata-only change.

Asset review covers fan badge recreations, generated franchise-inspired series
cards, QML-drawn clinic elements, and screenshots with separately supplied art,
sprites and logos. A creator's attribution/license, generated provenance or a
non-affiliation disclaimer does not by itself clear underlying game designs.
Factual compatibility, stable game IDs and independently written adapters should
not be removed merely because they refer to supported games.

The audit found no commercial ROM/BIOS/save files in the tracked path inventory;
Gitleaks reported no typical secret findings across its history scan. These are
bounded checks, not a guarantee that every historical byte or third-party right
has been cleared. Private scan outputs stay outside version control.

## Order after this pass

Resume MP-02 from [current tasks](CURRENT_TASKS.md). Public-readiness issues remain
explicit follow-ups and do not restart passed multiplayer checks, move Pack Studio
forward, or replace image/update work under #70/#71. The accepted GPL-3.0-or-later
decision is recorded; asset replacement and history-cleanup choices still require
the owner's decision first.
