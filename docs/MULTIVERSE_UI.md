# Multiverse presentation boundary

P1 adds a system-browser face within Worlds and a separate dark Home composition. This is the controller/UI foundation for #28/#31/#32/#43, not completion of the playable Multiverse library.

## Controls and isolation

- L1/R1 still switch the five primary pages. L2/R2 switch Worlds/Multiverse when no service, keyboard, notice or modal owns input. Each face retains its route, filter and selection while visiting another page.
- Multiverse uses system grid → title list → detail. B unwinds that route, never switches faces. X opens the shared controller keyboard in the title list; Y cycles All/Linked/Missing. An empty filtered list offers A to reset; an empty system offers A to return to systems.
- X switches the unobstructed Home between Pokémon and Multiverse. Keyboard, drawer, system menu and notices retain priority. Physical Home returns to the active Home context.
- The shared Y drawer shows only that Home context's choices. A selects without launching; Pokémon-aware companion pages continue to use the existing Pokémon context. Drawer positions are independent.
- Pokémon selection, ordinary launch, clean-exit pictures and stored navigation remain unchanged. Returning Home from Pokémon Worlds chooses the Pokémon Home; choosing a Multiverse sample opens Multiverse Home.

## Availability

`MultiversePresentation` owns transient presentation state only. It has no filesystem, repository mutation, process, adapter or save access. Its sample entries are fictional and appear only with the existing non-editable development library. Missing sample entries remain inspectable but cannot be selected; Home A on a selected sample explicitly reports that no game was launched. No play time, achievements or Pokémon progress are invented.

The real editable library gets empty Multiverse lists and an empty selector. Home A opens the system browser. As of 2026-09-23, production hides all six fixture system cards and shows the real empty state. They remain only in development preview. Real installation enumeration, file linking, media and launch are P3/P4 work. No loading task or integration is running behind this synchronous presentation; the UI does not show pretend loading or success.

**Owner clarification, 2026-09-23:** the P3/P4 registry must include every platform
with a credible Flip/Armada runtime route, not a fixed shortlist. Show only systems
with available ROM content; empty platforms stay hidden. Determine visibility from
the full content inventory, never the current title search/filter. Missing media
must not delete registrations, history or saves. Runtime readiness is separate
from file presence. The requested emulator/BIOS/four-ROM-per-system preparation
remains part of that connected delivery, not a claim that it already works.

Inventory starting points checked on 2026-09-23:
[Armada's supported emulators](https://armadaos.dev/emulation/emulators/) and
[Libretro's core/system table](https://docs.libretro.com/guides/core-list/).
Reconcile them with actual Linux ARM64 builds, controller routes and Flip results;
an emulator listed for Armada's wider device family does not prove acceptable
performance on this particular Flip. Keep incompatible/unverified routes separate
from verified launch capability; do not limit the registry to currently installed
cores or turn unavailable test ROMs into fictional installed games.

New Multiverse routes, Home face and sample selection last for the current application session only. They are deliberately not written into legacy single-owner navigation. P2 ownership and P3 domain identities must precede P4 durable per-Trainer/context choices and real library binding. Existing Pokémon persistence remains intact.

P4 also includes missing viable emulators, their required BIOS and four ROMs per supported system from the owner's server disk, with launch/input/save/return verification. [Execution order and collection acceptance](ROADMAP.md#later-collection-deliverable--representative-classics) remain authoritative; unavailable runtime/content gaps remain explicit.

## Verification

Interaction tests cover context isolation, selection without launch, missing-file actions, search/filter reset, per-system search memory, modal precedence and honest editable-library emptiness. The rendered Worlds SDL scenario covers both paired faces, title detail, Home/selector, keyboard, empty system recovery and return to the untouched Pokémon route. Local resize checks cover handheld and 1080p layouts; native compositor verification is separate from physical-button testing.

2026-09-20 verification: the Windows suite passed 35/35, with affected interaction/rendered scenarios rerun after the final empty-Home routing and animation-wait refinements; ARM passed 39/39 including separate persistence-process coverage. The SDL scenario also passed in Flip's native Gamescope session, and its 1080p Home/system-grid/selector captures were visually inspected. Drawer geometry is checked after its actual animation settles, with a bounded wait; software-rendering timing does not substitute for settled focus visibility. These automated controller events do not claim a new physical-button acceptance by the owner.

The non-testing build was installed on Flip with binary/database backups and hash readback. Injected InputPlumber events verified Home X, fixed A, both face triggers, list restoration, Guide and the empty Y drawer in the actual personal-library session. Captures confirmed no sample entries in production. Database integrity/schema 7, the existing Trainer profile and the 686-Adventure count were preserved; the device was left on Pokémon Home. Private captures, manifests and backup locations stay outside Git.
