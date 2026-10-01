# Physical Home menu — #112 / #49

The local menu reuses the existing shell, exit controller and owned-process
transport. It is separate from Start and from the Home primary page.

- In the shell, Home opens a compact Home / Friends / Chats selector over the
  retained page, face and focus. Home again or B dismisses it without navigating.
  Selecting a destination is explicit. Friends/Chats use the existing Social
  faces; this does not imply that their external provider is installed.
- During a supported Adventure, Home opens Continue / Exit game over a temporary
  game preview. Continue, B and Home again return to the same process. Opening
  the menu does not close play history, save, relaunch or request pause.
- Only Exit game calls the existing safe-exit controller. It hides the menu,
  retains exclusive input and captures a fresh game frame before the save question.
  Manual/unknown titles ask; verified autosave skips the manual question after a
  successful capture. Failed capture preserves the cancellable confirmation and
  previous valid media. No platform-wide autosave assumption is introduced.
- Start retains its system controls. It is not intercepted inside games or
  copied into Home. Native save writes still require a closed game and the
  existing owner/revision/transaction gates.

## Input, identity and recovery

The game presenter remains bound to the launch controller's actual session;
its title is resolved at game start, never from a mutable Home/library selection.
Ending a session or losing helper availability invalidates pending menu previews.
Menu captures use a separate generation and never enter exit media/history.

Helper protocol 2 supplies physical Home and vertical direction state in addition
to A/B, connection and full neutral state. Every layer/focus/lease transition
requires release. Down+A in one sample selects an action without activating it;
B/Home takes priority over A. Shell menu transitions also reset the central
controller neutral gate, including both sticks. Stable action positions cannot
be moved by presence updates; future capabilities must retain action identities.

Before either capture the helper checks that compositor focus belongs to the
owned game window, waits for its frame, and retains interception throughout.
Explicit exit never reuses the menu preview. Late worker results retain their
original capture kind/token; cancellation cannot relabel them. The independent
watchdog restores input on helper/UI failure without killing the game. Existing
graceful-close ownership and single-request safeguards remain.

## Remaining #112 work

Independent shader / ratio / exact supported widescreen / bezel controls are
deferred until after communication and multiplayer invitations by the owner's
2026-10-01 priority change. Their runtime capability, preview, per-game inheritance/reset and
live versus next-launch gates remain open. Unsupported controls are omitted.
In-game social/activity actions await their actual providers; no duplicate chat,
voice or save implementation is introduced here. Full issue acceptance remains
in [the register](EXPANSION_98_112.md) and [roadmap](ROADMAP.md).

## Verification

2026-10-01 delivery:

- Windows native build and five affected CTest entries passed: core input,
  interactions, exit lifecycle, presentation and rendered exit scenario.
- ARM64 exercised all 56 CTest entries in a network-isolated container. The
  initial run passed 54; the old Home interaction expectation was updated and
  the video test rerun with the headless Qt environment. The final eight-entry
  rerun passed, including helper recovery, transport and rendered Home/Exit.
  Production was rebuilt with `BUILD_TESTING=OFF`.
- Both live binaries match SHA-256
  `8f801f20ddbdc3486491000f14c18f812a46e7ff2c68bafda0e88a458e7b3429`.
  Both protocol-2 helper files were installed with backups; each device's own
  database, boot preference, library and nearby helper files were retained.
  Flip retains 3 Trainers / 830 registrations; Odin retains 1 / 25.
- Actual Gamescope captures show the shell menu and Emerald overlay on both
  handhelds, preserving their different shell themes and emulator appearance.
  Events were injected through the handheld input sources over SSH, not pressed
  by the owner. Continue/repeated Home and cancellation retained the same live
  process (Flip 439458, Odin 99199). Explicit Exit then returned both to TrainerOS;
  both history outcomes are `returned`, with no remaining emulator process.
- Reopened SQLite exit images have verified checksums and contain clean game
  frames, without the Home menu or save question. On Odin the retained emulator
  bezel/shader is part of that game image. No in-game progress was written for
  this check. The owner still has the normal physical-button feel check.

Private logs/captures are under `work/research/home112-*` and each device's
`~/traineros-home112`; they are not distribution assets. Synthetic autosave
evidence does not certify an actual game's autosave policy or an untested
runtime's focus/pause behavior. The provider does not request pause, but an
emulator's own inactive-window setting can still pause it; online runtime proof
remains separate.
