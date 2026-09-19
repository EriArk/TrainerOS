# TrainerOS Product Specification

The following feature sections describe the **accepted target** unless explicitly labeled implemented or historical. Exact reads/writes are per ROM/build; [provider and transaction acceptance](EXPANSION_42_62.md#exact-save-providers-and-research) forbids generation-wide claims from one title.

**Target reconciliation — 2026-09-19 (#62).** The accepted [#42–62 specification](EXPANSION_42_62.md) supersedes older product direction. Planned behavior below is not a claim that the deployed build has changed; see the [working baseline](ROADMAP.md#working-baseline) and dated module evidence.

The 2026-09-11 collection extension is specified in [Worlds collection catalogue](COLLECTION_CATALOGUE.md): region-first missing/owned editions, visible platform badges and controller file attachment. This intentionally extends the former installed-only library and exception to hiding all platform information; emulator commands and core details remain outside the normal UI.

## Purpose

TrainerOS is a software-only, controller-first **Linux handheld shell/session** for a Pokémon-focused Retroid Flip-class device running ArmadaOS.

It should feel like a dedicated personal trainer terminal rather than a generic Linux desktop or emulator frontend.

Normal flow:

**open/wake device → TrainerOS → browse progress or choose/continue an Adventure → configured emulator/application → return to TrainerOS**

KDE Plasma exists as explicit Desktop / Maintenance Mode and should not appear during ordinary use.

TrainerOS is the intended main/default mode. The accepted 2026-09-13 issue [#11](https://github.com/EriArk/TrainerOS/issues/11) replaces permanent Steam retention with reversible removal. The owner's subsequent clarification prefers Plasma Mobile as the only maintenance shell if viable on Flip. Existing Steam/Plasma Desktop remain until replacement and recovery are proven; shared KDE dependencies and personal data remain protected. See [session consolidation](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).

## Accepted next modules — planned, not implemented

The [roadmap](ROADMAP.md) integrates issues #1–62. [Earlier expansion acceptance](EXPANSION_PLAN.md) retains ownership, onboarding/PIN, media/audio/feedback, Help, catalogue, session and cleanup gates. [New acceptance](EXPANSION_42_62.md) adds shared Adventure/navigation, ordinary-save lifecycle, exact providers, Center/Journey, living Party and the artwork pipeline. These are accepted targets, not delivered features.

Multiple Trainers share device library/installations/media but own journal/history/Hall/selections and RA identities. A shared external save does not become a separate playthrough because a Trainer is created. Collection stays English-first and complete for substantial playable titles with credible Flip/controller routes, including missing/linkable editions and meaningful hacks; closed online-only titles are excluded. Reference curation (#30) never deletes private files/history (#18).

Adventure media, local selected-game video, original/licensed audio, hardware feedback/RGB, offline Help and supported boot branding retain their independent roadmap gates. Plasma Mobile replacement, reversible Steam removal and careful software/background cleanup preserve verified recovery and measured rollback. Sleep remains disabled/deferred.

## Vocabulary

- **World:** Pokémon region or explicit sourced spin-off grouping; region-first, not console-first.
- **Adventure:** stable playable title/build/profile, distinct from installation, ordinary save, Trainer and playthrough.
- **Choose Adventure:** shared recent selector; choosing changes context, never launches.
- **Trainer:** personal identity/records; shared library ownership is separate.
- **Multiverse:** non-Pokémon system browser paired with Worlds, with an independent Home context.
- **Pokédex:** offline reference plus current-save progression and separately sourced manual history/art.
- **Hall of Fame:** live Journey and preserved Champion/manual archive, paired with external RA.
- **Pokémon Center:** Pokédex companion for practical Party/Storage and safe ordinary-save services.
- **Desktop / Maintenance Mode:** deliberate KDE maintenance/recovery transition.

Platform badges and Multiverse system names are intentional normal information. Emulator commands, paths, cores and package details stay in advanced/integration surfaces.

## Primary sections

The five full-screen peer pages remain **Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**, selected with **L1/R1**. Home is not a permanent background shell.

**Planned #43:** L2/R2 switches paired faces within Worlds ⇄ Multiverse, Pokédex ⇄ Pokémon Center, and Hall/Journey ⇄ RetroAchievements. Preserve each face's route/focus/filter and the launch return route. B unwinds local detail; it does not flip the pair. Use a compact existing-header/chassis indicator, not a sixth page or large second tab row. Home retains its separate **X** Pokémon/Multiverse toggle (#31).

## Home

Home is a living trainer overview, not a launcher grid and not a giant Continue screen.

Useful Home modules may include:

- current Adventure and active World
- badge and World progress
- Pokédex Seen/Caught counts
- latest milestone or Hall of Fame activity
- recently discovered/caught Pokémon when available
- selected favorite/featured Pokémon
- time, battery, network, storage, backup/sync status

Keep the layout calm; not every module must be visible at once.

### Choose Adventure drawer

**P1 delivered:** the shared selector and Pokedex/Center pair now work on the single-Trainer baseline. [Implementation and remaining boundaries](SHARED_ADVENTURE.md). The full ownership/provider contract below remains the target.

**Planned #9/#49:** one shell-owned **Y · Choose Adventure** drawer selects the active Trainer's shared `CurrentPokemonAdventureContext`: Adventure ID, resolved exact build and ordinary-save identity/revision when available. Pokémon Home, Pokédex, Center, Hall, RA and Adventure-aware Trainer consume this same context. Multiverse Home remembers its own independent game selection. Before an explicit choice, use the latest actual launch in that Trainer/domain; unrelated launches do not overwrite an explicit choice.

A on a card commits the context and closes without launching; B cancels and restores the opener unchanged. L1/R1 closes the drawer without committing and switches primary page. Modal/keyboard/Start/recovery flows suppress shared Y. Worlds intentionally keeps its browser-local search/filter controls. Resolve existing local-Y conflicts explicitly when implementing the shared route. Unsupported save features never silently select another title.

Cards show the latest clean TrainerOS exit image, title, World and honest session/progress metadata. They are recent Adventure choices, not emulator-state slots. No extra persistent Current Adventure capsule/chip or independent per-feature selector: headers/content may show identity naturally.

Unobstructed Home A immediately invokes its large physical launch button regardless of prior directional input. Launch uses normal game startup and the game's ordinary save/autosave; Worlds also has an explicit launch action. Unconfigured/empty selections offer setup or Worlds exploration. The bottom-frame drawer remains compact, expands before rising inside the fixed viewport, and preserves controller focus. See [shared selection acceptance](EXPANSION_42_62.md#shared-adventure-and-paired-navigation).

## Worlds

Pokémon Worlds is the Adventure library organized by region first, not by hardware platform or emulator. The accepted Multiverse extension is a distinct L2/R2 paired system-browser face for non-Pokémon games; it does not turn those systems into Pokémon Worlds.

Normal application mode starts with reference regions and an empty personal library. Start → Manage Adventures provides controller add/edit, a local file picker, primary/additional World relationships and custom Worlds for ROM hacks. Saving stores TrainerOS metadata while the selected file remains external and unchanged. Records retain their identity when edited or relocated. See `LIBRARY_AND_LAUNCH.md` for the implemented flow and acceptance criteria.

Registration does not imply launch or progress support. Until a real adapter is configured and validated, detail offers a clear setup-needed state; Home/Choose Adventure does not invent play history or save observations. The ephemeral preview keeps sample Adventures separate from personal data.

Initial set:

- Kanto
- Johto
- Hoenn
- Sinnoh
- Unova
- Kalos
- Alola
- Galar
- Paldea

A World can show status, Adventure count, total playtime, badges, Pokédex completion, and last visit. Opening it reveals configured Adventures in that region.

An Adventure may contain user-facing title/progress data plus hidden integration metadata such as adapter ID, content reference, launch settings, ordinary-save references, and parser/provider configuration.

The model should support one primary World plus additional World relationships for titles spanning multiple regions.

Planned #56 uses larger/taller World cards with original cached environmental linework and data-driven themes. Intentional diagonal pairs may combine semantically related small groups without merging their identities; a title-count threshold alone never merges Worlds. Preserve readable type and dominant golden focus.

## Pokédex

Pokédex remains useful offline and controller-operated: bounded lists/detail, combined regional-collection/type/status filters, name/number search, sorting and clear empty/reset states. Search uses the shared keyboard with digits in a separate right block. Reference regional membership is not proof of a personal encounter.

**Planned #46** separates five layers:

1. Offline species/form/reference facts.
2. The selected Adventure's verified ordinary-save Seen/Caught as primary current progression, with only proven regional/National/form semantics.
3. Preserved Trainer-owned manual journal, Caught collection (#14), favorites and history, explicitly sourced and secondary to the current-save view.
4. Optional installed classic illustration artwork (#13) for the primary long list.
5. Optional PMDCollab animated sprites/portraits (#51) for detail and living-party scenes, not the main list artwork.

Unknown is distinct from false/not-caught. Aggregate counts, species flags and individual Pokémon are separate evidence levels; none fabricates catch dates/forms. Failed reads retain a labeled complete last-good snapshot for the same source. Save rollback does not erase manual/history records. Shared Y refreshes the same Adventure across features; L2/R2 preserves each Dex/Center route.

Art providers are separate from reference facts and game media. No official artwork is bundled; missing/partial packs use honest fallback. The artwork sequence is **#58 raw seed/completion → #60 real Flip mapping/profiles → #57 generic contract → #59 Pack Studio → Settings polish**, with no resident downloader. [Detailed acceptance](EXPANSION_42_62.md#artwork-sequence).

## Trainer

Trainer is the persistent personal profile. It may show:

- trainer name and avatar
- favorite/featured Pokémon
- current Adventure/World
- total playtime
- Worlds visited/completed
- championships and badges
- Pokédex Seen/Caught totals
- milestones

It should feel like an in-universe trainer card/profile rather than account settings.

### Create and edit Trainer — required functionality

The product must support creating the local Trainer profile when none exists and editing it later from Trainer. This is a functional requirement, not a decorative mock profile.

- Create a name, choose an original/user-provided avatar or emblem, and optionally choose a favorite Pokémon.
- Allow name entry entirely with the controller through an on-screen keyboard.
- Validate a non-empty display name; preserve input on validation failure.
- Edit existing profile fields with explicit Save and Cancel behavior. Back cancels the draft without altering the saved profile.
- Persist the profile through the repository layer and restore it after restart. Renaming must preserve the profile ID and its progress/history relationships.
- Refresh Home and Trainer after a successful save; a failed write must preserve the previously saved profile and leave a retry/cancel route.

Acceptance: create using physical controls, restart and recover the same profile, edit and save, cancel a second edit without changing saved data, and retain all existing progress. Multiple independent Trainers are now an accepted next stage in #19–20, after the lossless ownership migration; see [Trainer acceptance](EXPANSION_PLAN.md#trainer-ownership-onboarding-and-power). The existing single-profile implementation remains the baseline; chooser/PIN isolation is not yet delivered.

## Hall of Fame

**Planned #47/#48:** Hall's first face is a live save-backed **Journey Record**, useful before completion, plus preserved Champion/completed-run history. Show only proven title-specific badges, milestones, playtime and Dex totals, with larger original crystal badges; no universal eight-badge/percentage/date assumptions.

Champion snapshots preserve exact build/playthrough/source revision and verified historical team/progress. Current Party is not the historical winning team; older saves do not erase the archive. Observation time is not victory time. Manual memories/editing remain valid and explicitly sourced.

RetroAchievements is Hall's **L2/R2 companion**, following the shared Adventure through verified content/set matching. It remains an external account source independent from current-save or manual completion truth. Settings owns the active Trainer's account; unsupported mapping never substitutes another game. Same-account complete offline caches retain earned/unknown distinctions. Earning, earned-state UI and verified notification have separate gates (#12/#24/#25/U7).

Acceptance includes controller list/detail/Back, paired-face restoration, Y changes, unsupported/corrupt/rollback states, preserved manual/Champion history, two-Trainer isolation and offline/wrong-account rejection. [Projection acceptance](EXPANSION_42_62.md#pokédex-journey-and-achievements).

## Pokémon Center / maintenance services

**Planned #44/#53:** Pokémon Center is a first-class L2/R2 companion to Pokédex, using the shared Adventure context. It is not a new primary page or merely system-maintenance branding.

- Practical Party cards/list and details expose verified slots, level, HP/status, moves/PP and held items.
- Storage exposes the actual title's boxes/slots. Read browsing precedes separately proven reorder, Party/Storage moves and release; destructive actions require fresh explicit confirmation and the shared safe transaction.
- Heal/Backup/Restore services retain ordinary-save protection. Paid healing requires proven party/money fields, exact fee/balance and in-game verification; backup-only fallback remains useful.
- Link Counter trade/transfer/sale is later exact-pair, recoverable two-device work; sale means in-game currency only.
- Party remains a stable management UI. A separate Party Playroom provides optional interactions; Practice Battle copies real Party data into a read-only sandbox and never mutates saves or grants rewards.

Device/account/integration settings remain in Start/Settings. No second Center save picker. [Center acceptance](EXPANSION_42_62.md#pokémon-center-and-practical-party), [Link Counter](EXPANSION_42_62.md#link-counter), [Playroom/battle](EXPANSION_42_62.md#sprites-living-party-playroom-and-practice-battle).

## System menu

`Start` opens a TrainerOS system menu from any primary section. It may expose:

- TrainerOS settings
- controller mapping
- visual/accessibility preferences
- Pokémon Center services
- Adventure management
- audio/brightness/network shortcuts
- sleep/restart/shutdown
- **Desktop / Maintenance Mode**
- restart TrainerOS / recovery actions

The system menu is part of TrainerOS, not a desktop taskbar or application launcher.

## Desktop / Maintenance Mode

KDE Plasma remains installed as a deliberate advanced environment.

Use it for:

- files
- terminal/development
- package management
- manual emulator configuration
- network troubleshooting
- recovery
- advanced system settings

Normal users should not need Plasma for ordinary Adventures, Continue, Trainer, Pokédex, Hall of Fame, or common handheld controls.

TrainerOS must provide a clear way back from maintenance mode.

## Input and accessibility

- normal use must not require touch, mouse, or keyboard
- every interactive element needs a visible focus state
- D-pad and left stick work consistently
- `A` confirms, `B` goes back/closes
- `L1/R1` remain global primary-page navigation
- `Start` opens system menu
- no state should be communicated by color alone
- avoid tiny text/targets
- controller mapping should become configurable

## Persistence

Persist at minimum:

- current primary section
- useful per-page UI state/focus
- configured Worlds and Adventures
- Trainer profile
- recent Continue entries/cache
- Hall of Fame history
- local Pokédex progress
- settings and mappings

Ordinary game saves remain external source data. TrainerOS stores source-aware observations and protected backup references, not the sole copy of gameplay. Legacy state metadata exists only for safe #49 migration, never as a new target capability.

## Adventure lifecycle

**Accepted target #49, not yet implemented:** normal TrainerOS creates, manages and resumes no emulator savestates/ResumePoints, in either Pokémon or Multiverse. Ordinary game saves/autosaves are authoritative. Relaunch starts the game normally; the game loads its own save.

On user-requested exit, capture a clean gameplay screenshot **before** the overlay. Resolve exact title/integration policy `manualConfirm | autosave | unknown`; do not infer it from platform. Manual/unknown asks “Have you saved?” while the game remains alive. B returns to the same process; A confirms graceful exit. Per the owner's 2026-09-19 clarification, verified autosave also asks "Close this game?"; it never exits without A. The target trigger is the physical Home/Guide button instead of Start+Select, subject to the Flip routing gate. Start retains the shell system menu. Confirmation is a user assertion, not automatic proof of saving.

Exit images feed Home/Y/history with Trainer/domain/Adventure/session provenance. Cancelled attempts and crash/kill/battery loss cannot fabricate a confirmed exit or replace valid history with a false capture. Preserve prior valid media where appropriate and mark interrupted outcomes honestly. Capture failure leaves a usable exit/cancel path, never a state-thumbnail substitute.

Checkpoint the launching page/paired face/route/focus, restore it promptly on return, and refresh ordinary-save observations asynchronously. First prove capture, overlay/input ownership and cancellation with the still-running emulator on Flip. Migration retires only verified TrainerOS-owned obsolete state artifacts safely, preserving ordinary saves, histories and independent images. [Full lifecycle/migration acceptance](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

## Platform and session requirements

- ArmadaOS is the system base.
- TrainerOS is a native Linux application/session, not an APK.
- KDE Plasma remains a secondary maintenance/recovery session.
- The first development build runs as a normal full-screen Qt application.
- Only after stable crash/recovery and process lifecycle testing should TrainerOS become the default graphical session.
- Session/compositor details remain behind a platform layer because ArmadaOS can evolve.

## Development model

The first milestone is a complete controller-navigable mock of the product structure.

The confirmed development sequence is bottom-up in dependency order:

**native project skeleton → shared interface/controller skeleton → shared backend and persistence → individual modules → real integrations → optional supported-game enrichment**

This replaces the former top-down plan. Establish the shared foundation before deep feature integration, then complete modules one at a time. Automatic save parsing and RA integration follow their roadmap prerequisites. Only verified capabilities may be presented as working product features; unknown data must remain unknown.

Visual design is explicitly not frozen by the mock. Whole compositions may be replaced as real data and physical-device testing reveal better solutions.

## Original bootstrap non-goals — historical

Do not block the first UI milestone on:

- making TrainerOS the default OS session immediately
- a custom ArmadaOS image
- universal save parsing
- emulator-state integration (now superseded entirely by #49)
- cloud accounts
- social/trading/battle features (bounded Link Counter/Playroom/practice are now planned, with their own gates)
- perfect Pokédex coverage
- removing KDE Plasma
- hardware modification

## Product success test

A Pokémon fan should be able to pick up the device and understand it as a dedicated personal trainer terminal without needing to know that ArmadaOS, Linux, Plasma, emulator binaries, or filesystem paths exist underneath the experience.
