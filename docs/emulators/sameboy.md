# SameBoy — generic GB/GBC battery linked pairs

Pinned upstream: [libretro/SameBoy](https://github.com/libretro/SameBoy/tree/aa158a889a48b538a0302873704a34577c8eb67d),
commit `aa158a889a48b538a0302873704a34577c8eb67d` (libretro 0.15.4).
The upstream MIT licence and copyright notice remain with the build sources;
TrainerOS does not replace third-party licences. Its replacement boot ROMs come
from that source tree; this route does not bundle Nintendo firmware or games.

## Integration and ownership

The upstream `gb_link_2p` subsystem emulates two linked machines and exposes
separate SRAM through memory IDs `0x100` and `0x300`. RetroArch supplies rollback,
controller slots and the existing relay. The same exact ROM is loaded twice with
private `player-1` / `player-2` basenames. Each player sees/hears and controls only
their assigned machine. Ordinary play still uses Gambatte.

Eligibility uses validated cartridge headers: supported MBC1, ROM+RAM, MBC3
without RTC, MBC5 and MBC5 rumble battery types, with 8/32/64/128 KiB SRAM as
appropriate. It does not infer link support from a game title. Existing
DoubleCherryGB netpacket and volatile linked-pair profiles remain separate.
RTC, battery MBC2, special cartridges, infrared and four-machine adapters are not
added by this route. Explicit single-player metadata still excludes invitations.
The exact Hnefatafl build `f76a1a8f9292bd68c9330dc9f2721d9b516e03b5ecba1f19cf81d540f528d3bb`
is excluded: its handshake stalls in this pinned pair even without a network.

After invitation consent and the ordinary save/exit question, TrainerOS prepares
the guest's opaque SRAM automatically. Online uses Magic Wormhole's existing
encrypted PAKE channel; Nearby uses a one-shot TLS endpoint whose certificate
fingerprint and random token travel inside the accepted party. No address,
code, emulator menu or manual save transfer is required from either player.
Exact ROM/core/runtime/helper identities, party member/slot and boot identity
are checked before final launch. Two-member admission is fixed before preparation;
late join is disabled. Preparation is bounded, cancellable and tied to its parent.

The host's initial RetroArch state contains both SRAM images. Each client receives
both emulated machines, so the peer's SRAM is necessarily available inside this
temporary game session. This grants no semantic save capability to a presentation
adapter. Only the assigned machine's SRAM can return to its local original path,
through the existing preimage, backup, conflict and interrupted-return protection.
The other machine's SRAM is never installed as the user's save. Temporary save
sorting, overrides, remaps, runahead and automatic state loading are disabled;
ordinary configuration and RTC files remain outside the session.

## Maintained changes and build

[build.sh](../../packaging/emulators/handheld/build.sh) pins sources and applies
[sameboy-linked-pair.patch](../../packaging/emulators/handheld/sameboy-linked-pair.patch).
It builds ARM64 using `make -C libretro -j2 platform=unix MKDIR=mkdir`.
The patch:

- Adds player-only video layouts while retaining upstream per-player audio.
- Fixes an out-of-bounds LCD callback argument for the second machine.
- Extends linked rollback state with in-flight serial bits, both input matrices,
  pending CPU cycles and joypad stability/access flags. It initializes padding
  and permits the initial two-machine state before the first frame.
- Polls frontend input once per frame before updating both machines, avoiding
  different RetroArch frame boundaries for the two controller ports.
- Exposes `traineros-linked-pair-v1` for installation capability checks.

Single-machine serialization is unchanged. Linked temporary states have a new
78-byte frontend prefix and must not be used with an unpatched/different core.
The cable protocol and upstream clock scheduling are unchanged. A clock scheduling
experiment was rejected after local reproduction of the Hnefatafl failure.

Both handhelds have the versioned `sameboy-20261010.3` core, SHA-256
`f0f6d03669761544b90b95596894fec41dee311c3fe8be5c0ffd7fe7866b1533`.
The existing transactional core-bundle installer retains previous bundles and
configuration for rollback. Original DoubleCherryGB/gpSP artifacts are preserved.

[install-linked-save.sh](../../packaging/emulators/handheld/install-linked-save.sh)
creates a separate versioned Python environment from the fully pinned
[requirements](../../packaging/emulators/handheld/linked-save-requirements.txt),
recording `pip freeze` and `pip inspect`. The host's `retroarch.json` selects
`linkedSavePython` and `linkedSaveHelper` after verification. Installed helper
SHA-256 is `30e7225594b5217317b35adbc2f18a0a86c1f6f020e3c21fb05b516358f09dc4`;
the requirements lock is `8e79d0d70c79b727f00f32772fab57a5eb3a7cdfe5fb80d16f4487338fb5f417`.
Dependency notices/binary redistribution review remains part of #115; installing
the private validation bundle does not clear image redistribution acceptance.

## Verification and maintenance

[check-linked-sram.cpp](../../packaging/emulators/handheld/check-linked-sram.cpp)
uses original synthetic ROM programs, two distinct 32 KiB batteries and actual
core serialization. It checks initial serialization, SRAM restore, serial/input
rollback over 200 frames with seven predicted frames, and one input poll per
frame. The previous view-only core failed the rollback regression at frame 2;
the final patched ARM64 core passes. Other SRAM sizes are not a certification of
every mapper/game combination.

[check-linked-save.py](../../packaging/emulators/handheld/check-linked-save.py)
covers the 128 KiB bound, wrong certificate/content rejection followed by a valid
retry, parent cancellation and malformed input. Installed Online and Nearby
128 KiB round trips passed. Application tests cover lifecycle ordering,
cancellation, exact identities, own-slot return, conflicting variants and
original-file preservation. [Installed gameplay evidence](../HANDHELD_MULTIPLAYER.md)
is recorded separately from these checks.

For updates, review subsystem/memory IDs, cartridge rules, core option names and
serialization/input changes; rebase or retire the patch, rebuild with retained
sources/licences, run the core/helper/application regressions, then validate
paired gameplay and ordinary save readback. Compare helper dependency locks and
licences before changing the installation manifest. Roll out matching artifacts
to both devices only with no active emulator/save operation. To roll back, use
the retained core-bundle transaction and previous helper configuration; preserve
original saves and any recovery copies. Do not resume a linked temporary state
across core versions.
