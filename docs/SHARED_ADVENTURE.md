# Shared Adventure selection and paired navigation

## Delivered P1 slice — 2026-09-19

The committed Adventure choice now serves Home and the Pokémon Center save shelf. Y opens the same chassis-mounted drawer from Home, Pokédex, Center, Trainer and both existing Hall/RetroAchievements views. A commits a card and returns to its opener without launching; B cancels unchanged. L1/R1 cancels an uncommitted drawer and changes the primary page. Only Home's fixed A action launches the chosen Adventure.

Worlds retains its Y filter. Keyboard, journal/profile/archive/account editors, filter pickers, restore confirmation and system/error overlays retain priority. Select opens the Pokédex field journal, starts a Hall memory, refreshes the existing achievement view, or creates a Center backup. Existing X actions remain. Within editors Y retains its save/symbol action; Select does not submit a draft.

L2/R2 switches the first implemented pair: Pokédex and Pokémon Center. Either trigger switches once per press. B unwinds local detail/confirmation, never flips the pair. The Pokédex entry, filters and detail survive visiting Center; the chosen face survives primary-page navigation and persisted navigation restart. Start → Pokémon Center reaches the same face and selected-game shelf. Center X refreshes, Select creates a backup, and A asks before restore. There is no additional primary page or independent game picker.

## Hall / RetroAchievements pair - delivered 2026-09-19

Either trigger switches the second pair inside Hall without the old local tab row. Archive and RA remember independent list/detail/action positions across pair changes, primary pages and persisted restart. B never changes faces. Shared Y retains the opener and the same committed selection; local Select creates a memory or refreshes account records. Modal editors, account form, keyboard, system/error overlays and the drawer block paired switching.

Provider completion cannot replace the archive face; a submitted memory finishing after navigation updates the remembered archive detail without pulling the user away from RA. On return, missing archive/achievement rows recover to a visible list/action, disabled refresh loses focus, and account isolation still applies. The existing active navigation fields stay backward compatible; optional per-face view values add restoration without a schema change. No current-save Journey or new RA mapping is implied.

## Identity and asynchronous work

`ShellController::currentAdventureId()` resolves one committed selection. Before an explicit choice, the latest actual launch is the default. The persisted `homeAdventure` key stays backward compatible but its meaning is shared. An unavailable explicit choice stays unavailable instead of silently falling back to another game's save. History remains intact and Y can choose another Adventure.

Center resolves the ID through the library registration and ordinary-save service. Existing exact-content/config/save inspection tokens still authorize backup/restore; QML receives no external paths. Changing the choice invalidates the previous UI generation. An old operation cannot populate the new game's shelf; the new registration is inspected after it finishes. In-flight writes keep their original target, and errors remain reportable after leaving. Confirmation blocks Y and paired triggers; primary-page navigation cancels it without executing it.

SDL trigger axes use edges and engage/release hysteresis, with no held-trigger repeat. Disabled/reconnected input must become neutral, including both triggers, before delivering actions. L1/R1 remain shoulder buttons. Development keyboard equivalents: Z/C for the pair, Tab for Select, Q/E for primary pages.

## Remaining scope

This is the first useful #9/#43 consumer on the single-Trainer baseline, not completion of either issue. P2 owns per-Trainer/domain persistence and migration. Exact-build/save revision publication for all consumers belongs to provider phases. Pokédex remains its reference/manual journal; Trainer remains an aggregate overview; Hall and RA keep their existing records. Shared Y there changes the committed Adventure, not their data semantics.

Hall/RA paired routing is delivered over the existing historical records. Live Journey/Champion and selected-Adventure RA projections remain pending. Worlds/Multiverse and Home X have an isolated [P1 presentation](MULTIVERSE_UI.md); real Multiverse library binding and durable per-owner/domain choices remain pending. Its sample-only selector never modifies the Pokémon context. No new parser, party editor, healing, save format or achievement match is implied. Pokémon drawer cards still represent recent Adventures; unplayed library browsing remains in Worlds until its planned catalogue/context work.

## Acceptance

- SDL scenarios exercise Hall/RA independent return, editor/account modal priority and shared Y, plus paired triggers and shared Y above Pokédex/Center, actual focus, A selection, B cancellation, L1/R1, Start priority, backup/restore/undo and background completion.
- A delayed old shelf read cannot populate a new selection. Missing explicit installations never fall back to the latest game.
- Trigger tests cover hysteresis, holding, neutral gating on foreground return and existing Switch-style A/B mappings.
- Rendered checks cover 960×540, 1920×1080 and letterboxed landscape, with space for the drawer above the footer.
- Automated SDL tests and physical trigger routing are separate evidence; device delivery is reported with the increment.

## Delivery evidence

Windows passed 33/33 tests, followed by the five affected navigation/rendering/persistence checks after the Settings-return fix and the final interaction test. The final ARM64 source passed 37/37; the non-testing binary was installed on Flip with a previous-binary/database backup. Database schema 7 and all 686 library entries were retained. Rendered Center, Pokédex, Hall and Trainer layouts were inspected.

On the production Flip session, InputPlumber button events and bounded evdev trigger injection into its existing virtual Xbox target exercised the paired routes, shared drawer cancellation/selection and Settings return without launching a game. Triggers were released to their observed neutral value. This verifies the production SDL route, not a human physical L2/R2 press. That physical confirmation remains open. InputPlumber's D-Bus `SendEvent` method panicked on this installed version; do not use it for analog-input QA. Its normal button route and the TrainerOS session remained responsive.


The subsequent Hall/RA increment passed a fresh Windows 33/33 and ARM64 37/37 run and was installed on Flip. Actual-session injected controller events verified both faces, shared selection/modal priority and persisted restart, with Gamescope review. [Full Hall evidence and physical-input limits](HALL_OF_FAME.md#paired-route-delivery-evidence---2026-09-19).
