# TrainerOS target acceptance — issues 42–62

Accepted **2026-09-19** after reviewing issues #42–62 and the updated #6/#8/#9/#13/#15/#17/#28/#31/#40. This is a specification register, not another execution queue. [ROADMAP.md](ROADMAP.md) owns phases P0–P12 and preserves steps 1–10, U1–U13 and deferred commitments. This documentation increment implements **#62 only**; none of the features below becomes available merely because it is specified.

The deployed baseline still has a Home-only selector, emulator-state integration, manual Pokédex/Hall records and a backup service reached through Start. Those are implementation facts, not the new target. Dated device evidence remains valid for its tested revision. The closed #6 and old #8 Phase D are historical; #49 replaces further state-resume development.

## Shared Adventure and paired navigation

Sources: [#9](https://github.com/EriArk/TrainerOS/issues/9), [#28](https://github.com/EriArk/TrainerOS/issues/28), [#31](https://github.com/EriArk/TrainerOS/issues/31), [#43](https://github.com/EriArk/TrainerOS/issues/43). P1 shared routing; P2 ownership; P3/P4 domain consumers.

One `CurrentPokemonAdventureContext` per Trainer contains the Adventure ID, resolved exact build and ordinary-save identity/revision when available. Features subscribe to the same committed selection. Missing parsing support does not substitute a different title. Preserve independently selected Multiverse games and the established latest-launch default until an explicit selection exists.

| Control | Target behavior |
| --- | --- |
| L1/R1 | Five peer pages: Home, Worlds, Pokédex, Trainer, Hall of Fame. |
| L2/R2 | Paired faces: Worlds ⇄ Multiverse; Pokédex ⇄ Pokémon Center; Hall/Journey ⇄ RetroAchievements. No sixth page or large second tab row. |
| Home X | Pokémon ⇄ Multiverse Home; independent per-Trainer choices. |
| Y | One shell-owned Choose Adventure drawer on Pokémon Home, Pokédex, Center, Hall, RA and Adventure-aware Trainer. Multiverse Home has its separate game selector. |
| Drawer A / B | A commits context and closes without launch; B restores the opener without changing context. L1/R1 discards unconfirmed selection and changes primary page. |
| Unobstructed Home A | Immediately invokes the large normal-launch button, regardless of prior directional input. Worlds has its own deliberate launch action. |

Worlds intentionally retains browser-local search/filter controls, including Y. Keyboard, confirmation, Start, recovery and other modal flows take priority over shared Y. Inventory/rebind existing conflicting local actions explicitly during implementation. L2/R2 belongs to shell companion navigation only when that route is available; it must not steal emulator input or change faces through a modal. B unwinds local routes, never unexpectedly toggles the pair. Preserve route, focus, filters and return context on both faces. Unsupported Center/RA capabilities leave the parent page useful, with honest fallback.

The familiar mounted **Y · Choose Adventure** affordance is the shared context control. Do not add a persistent Current Adventure capsule/chip, independent save picker or duplicate selector. Existing page headers/content may naturally identify the Adventure; a compact paired-face indicator belongs in existing header/chassis space.

**Acceptance:** select Emerald in Pokédex and FireRed in Center; Home/Hall/RA follow the same ID without launching. Check cancellation, shoulders, local Worlds Y, keyboard priority, two Trainers, restart, missing/unsupported saves and both Home domains. Actual SDL events and Flip input must verify paired routes and launch/return restoration.

## Ordinary saves and screenshot-first exit

Source: [#49](https://github.com/EriArk/TrainerOS/issues/49); updates #6/#8/#9/#15/#17/#31. P1 lifecycle, before safe save writes and before exit-media consumers. Applies to Pokémon **and Multiverse**.

The target normal product creates, manages and resumes **no emulator savestates or ResumePoints**. Ordinary in-game saves/autosaves are authoritative. Relaunch is normal game startup and the game's own save loading; it is never advertised as exact instant resume. Emulator-internal maintenance capabilities are not a TrainerOS user-state model.

User-requested exit must:

1. Capture the clean gameplay frame **before** any TrainerOS exit overlay.
2. Resolve the exact title/integration policy: `manualConfirm`, verified `autosave`, or `unknown`. Platform alone never establishes autosave.
3. For manual/unknown, ask “Have you saved your game?” while the process remains alive. B (“Not yet”) removes the overlay and returns to that same process. A confirms graceful exit. Verified autosave can skip this question.
4. Commit exit media/history with its actual outcome, restore the launching route, and refresh ordinary-save observations asynchronously.

The prompt records the user's confirmation, not proof a save occurred. Pending captures from cancelled/failed attempts cannot masquerade as completed exits. A capture failure must remain honest, preserve the prior valid image and leave a usable cancel/exit route; no substitute state thumbnail. Crash, forced termination or battery loss cannot fabricate confirmation, a safe exit or a new verified screenshot. Preserve valid previous media/history where appropriate and report interruption.

The clean exit image is the canonical source for Home/Y/recent history. #15/#17 resolve images by Trainer, domain, Adventure, session and revision; retain the established same-context fallback only when its source is honest. An optional Hall image is explicitly selected, never proof of victory. No separate wallpaper database or state-thumbnail fallback.

**First implementation gate:** prove an exit request, capture and modal/controller-focus round trip while the emulator is still alive in the actual Gamescope/supervisor arrangement. Today's hidden shell/process-stop path is not proof of a working overlay. Cancellation must not terminate/relaunch the game; do not claim universal overlay support from one adapter.

**Migration:** stop exposing old state slots and launch dependencies; preserve ordinary saves, histories and independently sourced images. Inventory legacy references and retire only demonstrably TrainerOS-owned obsolete artifacts with a reviewed recovery path. Do not delete user/emulator files by extension or drop unrelated metadata. Existing resolver coupling to a resume profile must be removed without losing ordinary-save identity safeguards. Keep legacy tests/evidence labeled historical until the replacement is proven.

**Acceptance:** manual title capture-before-prompt, B same-process continuation, A graceful return, unknown asks, verified autosave skips, no overlay in image, Home/Y provenance, normal relaunch, migration/restart/rollback and interrupted outcomes. Physically verify a manual Pokémon title and a verified autosave title when available; absence leaves that specific gate open. Sleep work remains deferred.

## Exact save providers and research

Sources: [#42](https://github.com/EriArk/TrainerOS/issues/42), [#50](https://github.com/EriArk/TrainerOS/issues/50). P1 minimal contracts, P8 incremental providers; research accompanies each provider rather than blocking all work.

Extend existing `GameProgressProvider`, save resolution and backup boundaries. Introduce only the shared contracts needed by the first exact-title consumer; no speculative universal save editor. Identity includes ROM hash/revision, platform, runtime/configuration, save container/format, resolver provenance and stable save fingerprint. Hacks, translations and revisions are separate builds until verified. “Gen III supported” cannot follow from one Emerald fixture.

Planned semantic snapshots: `TrainerSaveSummary`, `PokedexSnapshot`, `PartySnapshot`, `StorageSnapshot`, `PokemonRecord`, `JourneySnapshot`, `ChampionSnapshot`. Carry provider/version, build/save identity, source revision, availability and observation time. Optional fields remain unknown. Pokémon identity, OT/origin, moves/PP, items, HP/status and box slots are exposed only where verified; aggregate Dex counts never synthesize individuals. Preserve format-specific unknown bytes needed for a safe round trip.

Independent read capabilities cover Trainer identity, playtime, money, badges, Seen/Caught, Party, Storage, individual records, milestones and Champion state. Independent writes cover heal, reorder, Party/Storage moves, release, money changes and import/export. A valid reader is not permission or evidence for its corresponding writer.

Every write shares one guarded transaction:

1. Resolve exact source and refuse active/uncertain external writers.
2. Read stable bytes; create and verify a protection backup.
3. Produce a separate candidate; independently validate checksums and the allowed semantic delta.
4. Recheck source identity/revision immediately before atomic replacement where the format/storage supports it.
5. Read back and verify; retain durable recovery and an honest result if failure occurs after replacement.

Maintain a machine-readable per-build capability registry and human evidence matrix as providers land. Each capability needs valid/corrupt/partial/unsupported fixtures, rollback/replacement/race checks and physical read comparison; writers additionally require in-game verification. Multi-file formats need their own recoverable protocol, not an assumed single-file atomic rename.

#50 research uses public ready saves only as private samples with source/author, hash, exact title/build and container provenance. Load each in its matching game and check claimed completion; “100%” is not ground truth. Add fresh/intermediate and controlled before/after samples to isolate fields. Keep research copies separate from personal playthroughs, originals intact and commercial saves outside Git; commit generated fixtures where appropriate. Download saves, not bundled executables or ROMs. Record save policy for #49 from evidence. Research availability does not certify a parser.

## Pokémon Center and practical Party

Sources: [#44](https://github.com/EriArk/TrainerOS/issues/44), [#53](https://github.com/EriArk/TrainerOS/issues/53), existing [#10](https://github.com/EriArk/TrainerOS/issues/10). P8 after shared context/navigation and exact reads; safe writes follow separately.

Center is a first-class **Pokédex companion**, not optional system-maintenance branding. It consumes the same Adventure, with no second current-save selector. Ordinary backup/restore remains useful when semantic reads are unsupported. Device/account/configuration services stay in Start/Settings.

- **Party:** stable practical cards/list and details for real occupied slots, with verified level, HP, status, moves/PP and held item. Unknown/empty/damaged states remain distinct. Sprites may decorate cards without making selection chase moving actors.
- **Storage:** actual title-specific boxes/slots, not a universal invented box count. Read browsing first; reorder/Party-to-box/box moves only with proven writers. Prepare a batch candidate rather than rewriting on each cursor move. Release needs explicit stronger confirmation and fresh input; never raw-zero an assumed slot.
- **Services:** Heal, Backup and Restore; paid healing shows exact fee and before/after balance from verified money/party fields, then uses the shared transaction. Restore refreshes context revision and all affected projections without erasing historical records.
- **Link Counter:** later exact-pair transactions, below. Party Playroom is a separate deliberate entry retaining management route/focus; Practice Battle never writes the save.

**Acceptance:** navigate real and unsupported Party/Storage entirely with controls; verify fields against the game. Exercise no-op, invalid/empty/full slots, write races, active emulator, confirmation cancellation/repeat, protection/replace/readback failures and restore. Physical proof applies separately to each writer/build, not to the whole Center.

## Pokédex, Journey and achievements

Sources: [#46](https://github.com/EriArk/TrainerOS/issues/46), [#47](https://github.com/EriArk/TrainerOS/issues/47), [#48](https://github.com/EriArk/TrainerOS/issues/48). P7/P8 save projections; P6 external RA can progress independently after context/account proof.

Pokédex layers offline reference facts, selected-Adventure verified Seen/Caught, the preserved manual journal/history, classic illustration art, and optional animated detail sprites/portraits. Save-derived status is the **primary current progression view**. Regional/National/form semantics require evidence; unknown is not false. Counts, species flags and individual records remain different evidence levels. Manual Caught (#14), favorites and notes remain a clearly sourced secondary collection, not silently merged truth. Save rollback never erases journal/history. Failed refresh retains a labeled last complete verified snapshot for the same identity, not zeroed or partly mixed data.

Hall's first face is a live **Journey Record** before completion: actual badges, milestones, save playtime and Dex totals with title-specific goals. Use a larger original crystal Badge Case; no universal eight badges, invented completion percentage or acquisition date inferred from a boolean. Preserve verified Champion/completion snapshots, exact build and playthrough lineage, with known historical team/progress separate from today's Party. Older saves do not erase the archive; distinct runs stay distinct, and observation time is not victory time. Manual Hall entries remain explicitly sourced.

RA is Hall's L2/R2 companion, following the shared Adventure through exact content/set matching. It remains an external account truth independent of save/Journey/manual completion. Account management is Settings → Accounts for the active Trainer. Preserve complete same-account offline caches; failed refresh must not turn earned into locked. #12 earning, #24 earned-state display, U7 matching and #25 verified notifications remain separate gates. Unsupported mapping must not quietly select another game's set.

**Acceptance:** one Y selection refreshes all relevant projections; rapid switches reject stale responses. Exercise unsupported exact builds, corrupt/replaced/rolled-back saves, known zero versus unknown, two Trainers, persistent manual/Champion history, offline RA and wrong-account/set rejection. Paired routes retain their own focus/filter state.

## Sprites, living party, Playroom and Practice Battle

Sources: [#51](https://github.com/EriArk/TrainerOS/issues/51), [#52](https://github.com/EriArk/TrainerOS/issues/52), [#53](https://github.com/EriArk/TrainerOS/issues/53), [#54](https://github.com/EriArk/TrainerOS/issues/54), [#55](https://github.com/EriArk/TrainerOS/issues/55). P5 assets, P8 exact Party consumers; each is separately deliverable.

#51 adds an optional external PMDCollab full-body sprite/portrait provider and bounded cache, with per-asset source/artist credit. Verify applicable terms and distinguish original contributions from official assets; attribution is not blanket redistribution permission. Exact form/shiny fallback is explicit; do not fabricate recolors. Semantic positive/concern/hurt reactions select available frames with quiet fallback. Classic #13 illustrations remain primary in long Pokédex lists; animated assets serve detail, Home, Playroom and practice scenes.

#52 uses lightweight native finite-state behavior and steering for only the actual observed Party. Bounded host areas/exclusion zones, idle/wander/sleep/social scenes, deterministic test seeds and one cached shared update tick suffice; no Godot/Unity, physics or general navigation framework. Home is quieter than Playroom. Pause offscreen/hidden/in-game, respect Reduced Motion, preserve text/focus and measure Flip frame pacing/idle load. Animation is presentation, never a claim about save friendship, personality or status.

#53 keeps management practical; #54 supplies the **separate Party Playroom**. Use a stable controller actor index for select/call/pet, approach/pause/reaction behavior, short social scenelets and frame-mounted sliding portrait panels on the left/right. No touch chasing or invisible moving focus. Y context changes rebuild the correct Party; Back restores management. Petting never changes HP, friendship or any save field.

#55 is a **read-only 1v1 practice sandbox** using copies of two real Party records. Verify exact title/generation/hack battle rules and required fields; unsupported data is unavailable or explicitly approximate, never silently canonical. No save mutations or real rewards: HP/PP/items/EXP/money/friendship/evolution remain untouched. Evaluate pkmn/engine and Pokémon Showdown through an ARM64 dependency/performance/license spike and semantic adapter; do not assume a library covers every generation. No browser battle UI. Simple AI or two-side controls first; coaching is optional later.

**Acceptance:** missing/form assets, credits/cache bounds, fixed management focus, reduced motion, hidden pause, bounded actors and rapid Adventure changes; Playroom remains usable without an art pack. Practice tests include deterministic rules, cancellation/return and byte-identical external saves before/after. Flip engine/runtime and sustained animation checks precede enablement.

## Link Counter

Source: [#45](https://github.com/EriArk/TrainerOS/issues/45). Later P8 extension after exact read/write and durable recovery, not a prerequisite for ordinary Center use.

Bluetooth trade/transfer/sale is between explicitly selected, paired and confirmed devices. Sale uses **in-game currency only**. Discover minimal non-secret identity; exchange validated semantic Pokémon records, never raw save chunks. An explicit exact-build pair matrix governs supported transfer fields, moves/forms/items and OT/origin preservation. Reject incompatible or lossy conversions; cross-generation compatibility and trade evolution require their own later provider proof.

Before UI completion, prove durable transaction IDs, protection copies, prepared candidates, source revisions, both sides' READY state, commit/readback receipts and recovery from disconnect, replay or one-sided commit. An unreachable peer can leave an explicit in-doubt transaction; never announce success after only one save changed or promise unconditional remote rollback. Sale additionally needs verified money/import/export/removal capabilities and bounded balances. No active game writer on either side.

**Acceptance:** two fake endpoints cover every interrupted stage, retries and idempotence; then two physical devices and in-game verification prove each supported pair. Lacking a second device leaves that gate open. Same-device transfer and broader evolution rules remain later extensions.

## World card atmosphere

Source: [#56](https://github.com/EriArk/TrainerOS/issues/56). P4 with #16/#30; can progress independently of save parsing.

Make World cards larger/taller, with low-contrast schematic environmental motifs filling their background: forest/heritage, sea/volcano, mountains and other region-specific original linework. Use data-driven themes and cached vector/material rendering. Golden selection remains dominant; do not recover space by shrinking text or per-frame drawing.

Small groups with roughly fewer than three or four substantial titles may share an intentional **diagonal split** card only when the pairing is semantically sensible. Retain each group's identity, title and focus/entry route; count alone must not merge unrelated Worlds. Large important regions remain standalone. #30 eligibility and U4 archive completeness still apply independently.

**Acceptance:** zero/one/many games, meaningful hacks, split/standalone cards, all themes, full focus glow, controller routing and physical landscape readability. Decorative maps imply no unverified geography/progress.

## Artwork sequence

Sources: updated [#13](https://github.com/EriArk/TrainerOS/issues/13), [#57](https://github.com/EriArk/TrainerOS/issues/57), [#58](https://github.com/EriArk/TrainerOS/issues/58), [#59](https://github.com/EriArk/TrainerOS/issues/59), [#60](https://github.com/EriArk/TrainerOS/issues/60), [#61](https://github.com/EriArk/TrainerOS/issues/61). Independent P7 art track; does not wait for save writers or Link Counter.

**Required order: #58 → #60 → #57 → #59 → production Settings polish.** No daemon/resident downloader. Do not build Studio or freeze final filenames before actual Flip layout validation.

1. **#58 PC raw corpus:** import the owner's [Gen 1–9 Drive seed](https://drive.google.com/drive/folders/1T2hF3ieas4mNBKQN6v94mlY8lbwT4KLx?usp=sharing); local folder/archive is a normal input, direct Drive access optional. Compare against the existing TrainerOS reference inventory, then fetch only missing artwork from the configured source (initially Bulbagarden), verifying source access/rate policies during implementation. A one-shot Windows/Linux utility retains originals, hashes, source names/provenance, provisional species/form mapping, missing and ambiguous reports; manual reruns resume by hash/cache and terminate. Ambiguity requires candidate review, never first-filename wins. Keep this raw corpus outside Git and tracked assets; no upload implied.
2. **#60 real Flip bootstrap:** place the completed corpus in development app data and use a temporary importer with existing species/form IDs. Exercise the real Pokédex at 960×540 logical / 1920×1080 physical, full-dataset scrolling, unusual forms/aspects/tall/wide art and transparent margins. Derive canonical filenames and slot geometry **from that rendering**, then export `pokemon-art-targets.json`, `artwork-target-profiles.json` and a coverage/layout report. Profiles specify list/detail/picker dimensions, alpha, safe area, bleed, fit/crop/contain rules, minimum resolution, runtime encoding/cache and measured performance. Preserve originals and nondestructive anchors/scale; no image-specific QML offsets. No final schema dependency here.
3. **#57 generic contract:** freeze versioned ZIP/folder manifest, stable species/form slots, hashes, authors/provenance, originals/transforms/runtime derivatives and profiles from #60. Partial packs are valid. Define exact-form, explicitly compatible primary/secondary fallback and neutral placeholder behavior. Shared validation rejects traversal/absolute paths, bad hashes/images and excessive counts/bytes; activation is atomic and preserves the prior valid pack. No ROM/save writes. Establish install/verify/remove mechanics now, but defer polished Settings UI until after Studio.
4. **#59 Pack Studio:** a separate Windows/Linux Qt/C++ desktop artist tool, intentionally mouse-friendly; normal use needs no Python, Node or browser. Consume the same validator/schema and Flip-proven inventory/profiles. Provide batch mapping, missing/review/next workflow, nondestructive fit/drag/zoom, immediate slot previews/safe-area warnings, autosave, credits and validate/export/reimport. It must support future original art without depending on #58 at runtime or inventing independent dimensions/form keys.
5. **Settings manager:** controller install/select/verify/update/remove, coverage, authors and storage; offline use and failed/cancelled updates retain a valid pack. Optional local official illustrations are never bundled in Git/default distribution. Record actual per-file authorship; neither a “Sugimori” label nor attribution grants redistribution rights.

Reference facts, illustration art, sprite animation and Adventure media remain separate provider identities. The bootstrap is private acquisition/research, not a permanent runtime service. Contract tooling/metadata can be committed; copyrighted images and private path manifests cannot.

**Acceptance:** stage outputs exist and are verified before moving to the next stage; ambiguous mappings stay explicit, physical profiles are reproduced in Studio, exported packs reimport safely, and missing packs never prevent Pokédex use. Network acquisition precedes device/schema work; actual device/profile proof precedes Studio. If a stage is blocked, continue an independent ready roadmap slice rather than silently skipping it.

## Reconciliation record

Source: [#62](https://github.com/EriArk/TrainerOS/issues/62). This docs-only increment changes the target, not the running app or GitHub issue states.

| Superseded requirement | Replacement / preserved evidence |
| --- | --- |
| #6 and #8 Phase D state-based resume expansion; U6 additional state providers | #49 normal saves, exit capture/confirmation and safe retirement. [Resume contract](RESUME_POINTS.md) and [GBA validation](RETROARCH_RESUME.md) remain historical. Ordinary backup maintenance stays U6. |
| Home-only selection and deferred shared Y | Updated #9 shared context; #20 owner migration; Worlds local exception. Existing Home A semantics retained. |
| Unspecified Multiverse local entry; Hall internal selector | #43 paired faces with L2/R2; Home X remains #31. |
| Center only backup/system metaphor | #44/#53 first-class practical Party/Storage/services plus separate #54/#55 playful surfaces. System services stay in Settings. |
| Manual-only current Dex; Hall only completed manual archive | #46 selected-save progression and #47 live Journey/Champion snapshots; manual history preserved; #48 external RA independent. |
| Primary animated art in long Dex list or generic installer first | #13 classic illustrations, #51 detail sprites; #61 strict bootstrap/device/contract/Studio/Settings order. |

Audit scope: all tracked Markdown, including initial CODEX_START, adapter/session/backup/feasibility documents and Help acceptance. Legacy type names and test records remain only under visible implementation/historical notices; active future tasks no longer expand state resume. Runtime Help does not yet exist: #40 must describe delivered capabilities and clearly label planned ones as each feature lands.

Documentation acceptance: compare README/Product/UX/architecture/domain/roadmap; check relative links/anchors, all issue destinations #1–62, all steps 1–10 and U1–U13, optional/classics/recovery/no-sleep commitments, and stale-term occurrences in context. No native test rerun, device deployment, content acquisition or feature implementation is part of this increment.
