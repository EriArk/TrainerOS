# Odin 2 bring-up — 2026-09-28

The owner's device was already running ArmadaOS 20260927.eba5232, kernel 7.2.6,
on the actual AYN Odin 2 device tree. This increment installed TrainerOS; it did
not flash an OS, repartition storage, replace Android partitions, remove software,
or copy Flip's Trainer database, accounts, secrets or game saves.

## Installed and preserved

- Native ARM64 production binary, ordinary desktop application and an additive
  TrainerOS Gamescope session at 1920×1080. A fresh EriArk profile was created
  through controller text entry. Private illustration and sprite providers were
  copied separately; absent Pokémon saves remain unknown in the interface.
- The existing SD Emulation/roms tree is reused through a home-directory alias.
  The shared 105-category preparation tool created 96 missing folders, honoring
  existing aliases. It moved/deleted no existing game. Empty categories stay
  hidden. Existing games were discovered; a private LocoRoco test copy was added.
- Existing RetroArch, PPSSPP, Dolphin and melonDS installations were reused with
  TrainerOS bindings. Their existing emulator settings and ordinary saves were
  retained. A binding is not per-title compatibility or guarded-exit proof.
- Existing Steam library, Plasma maintenance choices, OdinCare/charging helpers
  and emulator launch helpers remain installed. No default-session service
  override was installed. The temporary SDDM choice used for the live session
  test was backed up and removed; the original Steam boot policy remains.
- Desktop has both the ordinary TrainerOS app and **Return to TrainerOS**, which
  enters the dedicated session. TrainerOS Start exposes Steam and maintenance
  transitions. Backups of replaced files and the initial database remain private
  on the device; no private artifacts are in Git.

## Input and launch evidence

Odin's built-in USB pad is 2020:3001, `AYN Odin2 Gamepad`, axes 0–5 with triggers
2/5. Flip's matching vendor/product uses a different axis profile. Odin also has
a separate paddle input source. The exit helper selects the verified pad and
retains a read-only ACL, not general access to all input devices.

Steam can leave InputPlumber on `deck-uhid`; its Steam-provided controller then
disappears outside Steam. The TrainerOS session selects Armada's `xbox-elite`
target only for the recognized Odin composite. Re-entering TrainerOS after a
Steam transition recreated the virtual Elite pad and restored controller
navigation without modifying a global input profile.

On the actual Odin, injected kernel controller events exercised profile entry,
top-level and secondary navigation, Worlds, Dex, Start and launch/exit. LocoRoco
ran in standalone PPSSPP 1.20.4; game input advanced its language/title sequence.
Home opened the small overlay, B resumed the same process, and a subsequent
Home/A exited and returned to the wheel. Its exit JPEG persisted and survived
session restart. These are device-path checks, not a claim of a human physical
button acceptance pass. The owner can do that next.

The Steam transition started Steam successfully; the captured intermediate
screen was its installation-verification splash. TrainerOS re-entry then passed.
A full Steam game launch, cold reboot, physical volume keys, suspend, other
standalone exit paths and two-device Link are not claimed verified here.

## UX audit and checks

Start keeps one action column with radios above sliders; Connections uses the
existing two-pane Settings rather than another submenu stack. Launch has only
its own footer, eliminating overlapping controller legends. Empty Home no longer
prints a redundant milestone under the Choose Adventure tab. The existing
Playroom composition is unchanged.

Actual-device screenshots: Start, Connections and PSP wheel/exit on Flip;
Dex, Worlds, PSP exit and restored input on Odin. Windows native and ARM64
production builds passed. Focused standalone/history/device tests, shared core/
interaction and exit/QML tests passed; four Linux overlay tests passed on Flip,
including Odin trigger-neutral sampling. Adapter knowledge/export checks passed.
No repeated full-suite or GitHub Actions delivery gate was used.

Both devices run production SHA-256
`bc68663a830535bfb6fe7c0b026b85fd9e5e263e662044053c46e23501c68a9a`.
Network scope and remaining acceptance are in [DEVICE_NETWORK_PLAN](DEVICE_NETWORK_PLAN.md).
This second installation enables later #45 work; it does not complete Link,
image distribution, OTA or sleep acceptance.

## Follow-up — 2026-09-28

Both devices now run
`b0b1f608b70cb85e399bd984478ec44fd10958dce9af7f3fcc558f3214773d19`.
Connections adds inline real Wi-Fi/Bluetooth management and masked controller
text entry. Actual-device scans, list/focus/face navigation and keyboard
cancellation passed; no saved networks or paired accessories were removed.
See the [implemented flows and remaining hardware gates](DEVICE_NETWORK_PLAN.md#connections-delivery--2026-09-28).

Odin's previously installed ARMSX2 is connected to the PS2 library route.
The Matrix: Path of Neo CHD passed short-A launch, Home question, B cancel and
confirmed graceful exit/return. Emulator settings, BIOS, memory cards, existing
Steam default and Plasma recovery remain intact. This does not validate a whole
playthrough or semantic PS2 saves. [Launch contract](ROM_PLATFORMS.md#native-ps2-correction-and-wider-setup--2026-09-28).

Native and ARM64 builds passed. All 44 native CTest cases passed across the main
run and necessary reruns: concurrent build/test file locks were resolved before
rerunning affected cases, and the old diagnostics scenario was corrected for
the added Start radio row. Focused device/standalone/diagnostics checks passed
after that correction. Five Linux overlay and four network-helper tests passed;
adapter knowledge/export checks passed unchanged. No GitHub Actions gate.
Actual captures include `odin-network-wifi.png`, `odin-network-bluetooth.png`,
`odin-network-keyboard.png`, `flip-network-wifi.png` and the PS2 launch/exit
captures in the ignored research directory. No private content is committed.

## Mainline Pokémon test library — 2026-09-28

The owner narrowed the second-device collection to mainline handheld games,
without ROM hacks or spin-offs. The bounded set is Red/Blue/Yellow,
Gold/Silver/Crystal, Ruby/Sapphire/Emerald/FireRed/LeafGreen, and
Diamond/Pearl/Platinum/HeartGold/SoulSilver/Black/White/Black 2/White 2.
The inspected private source's 3DS directory contains no ROMs; no 3DS game is
claimed installed. Extra copies from the interrupted broader transfer were
removed only from this increment's manifest. Source originals and Odin's
pre-existing games remain intact.

All 20 destination ROMs passed SHA-256 comparison (2.09 GiB total). The final
library contains 3 GB, 3 GBC, 5 GBA and 9 DS registrations and no installed hacks;
SQLite integrity and foreign-key checks passed. Existing World creation caused
six automatic-import failures; the bounded device import restored those exact
records and source World memberships. Fixing idempotent World creation remains
library follow-up; this preparation does not claim that scanner bug is fixed.

The matching full English Emerald ordinary save was copied separately from
Flip, without copying its Trainer identities, accounts, history or backup lineage.
Odin's normal emulator save location is used for its original EriArk profile.
TrainerOS uses a separate RetroArch base configuration with automatic overrides
disabled, preserving the existing external emulator configuration. The installed
mGBA override otherwise blocked the verified save route. Gambatte was added for
GB/GBC; existing mGBA and melonDS installations were retained.

On Odin, short A launched Emerald, its Continue menu displayed 8 badges and
386 Pokédex entries, and the full save loaded. Guarded Home exit returned to
TrainerOS. Home read 8/8 badges and 386 caught; Party displayed all six level-100
members. Actual-device captures are retained privately. These are preparation
and representative launch/readback evidence, not a two-device multiplayer/Link
test or proof of every title's compatibility. The existing #45 gates remain.
