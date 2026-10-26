# Flycast native Online / DCNet

Separate from `flycast-netplay` (Dojo GGPO), `flycast-lan` (cabinet link), and
ordinary Flatpak Flycast. Never install this over those directories.

## Reproduce

Pinned Flycast v2.7: `5aa091fde632fb332c8d8c34e280d62dc951954c`.
`build.sh NEW_WORK NEW_DEST` checks out that revision and its submodules, applies
`traineros.patch`, builds Release with host SDL2, OpenGL and Vulkan, and retains
upstream's license. Required development packages: C++ compiler, CMake, Ninja,
Git, SDL2, asio, curl, udev, Vulkan loader/headers, GL/EGL, ALSA, Xrandr and Xi.
The validated build uses Fedora 44 ARM64. Do not reuse another task's build root.

Complete the new destination with:

```
python3 bundle.py NEW_DEST EXPLICIT_OWN_FIRMWARE_DIRECTORY --renderer vulkan
```

This copies only the owner's explicitly supplied `dc_boot.bin` and `dc_flash.bin`.
No firmware, discs, accounts, keys or saves are included in this repository.
The manifest hashes native binary, catalog and firmware; the shell verifies them.
Installation root: `$XDG_DATA_HOME/TrainerOS/emulators/flycast-online`.
Host dependencies must pass `ldd` before swapping the shell/package.

## Bounded upstream patch

- `TRAINEROS_DC_INSPECT=1` decodes the supplied disc before window/input/CPU/network
  startup and prints its ten-byte IP.BIN product number as hex, then exits. It
  uses a fresh private config/data directory; no ordinary saves are loaded.
- `network.TrainerOnline=yes` assigns the session controller to Dreamcast port A.
  Other runtime configurations retain upstream's original enumeration.
- `TRAINEROS_DC_SESSION=1` limits Flycast fallback directories to its private config/data.
  Global XDG_DATA_DIRS stays intact so Vulkan can discover the installed driver.
- DCNet networking, modem/BBA, game servers and save formats remain upstream.

## Catalog

`catalog.py INPUT_DAT OUTPUT_JSON` derives product aliases from the pinned public
[libretro Redump DAT](https://github.com/libretro/libretro-database/blob/9a899f5b13ed1c355491e342203be29bc573323b/metadat/redump/Sega%20-%20Dreamcast.dat).
Input SHA-256: `010a5b393fc6f0aee22fc9a1f6849bb7b7fa9a08ab75ac4602a810a1c3d23fa3`.
Published service families come from the [DCNet author documentation](https://github.com/TheArcadeStriker/flycast-wiki/wiki/DCNet)
and [Flycast releases 2.5–2.7](https://github.com/flyinghead/flycast/releases).
Catalog titles are used only when generating the package, never to infer support
from a user's filename/title. Runtime reads the native disc's code. Hyphens/spaces
and Sega's numeric MK prefix are normalized; ambiguous aliases fail generation.

The initial catalog has 71 aliases and 35 distinct catalog labels, including
regional name variants. These are candidates, not 71 certified games or live
service checks. Dee Dee Planet, Internet Game Pack and PBA Bowling have no matching
serial in this DAT. Propeller Arena has no serial; Alien Front Online/Bomberman
Online need a current DCNet-specific service confirmation. Do not silently label
these games incompatible or enable them from names alone. Add reviewed exact
codes/alternative content identities with their source, not device-specific rules.

## Update / rollback

Rebase only the bounded patch hunks against a reviewed upstream revision;
update the shell's pinned source gate, package manifest and service evidence in
the same change. Run the Flycast save/lifecycle and shared runtime suites, native
headless disc checks, production build, and `trainer_flycast_online_probe` against
an owned disc and a NEW diagnostic save root. Probe arguments: disc, diagnostic
save root, installed adventure-overlay.py, report prefix. It mutes its private
session, captures Home and confirms graceful exit; it does not join a game server.
Run again with the same diagnostic root to check persisted VMU/flash reopening.

Install atomically only while no game/save transaction is active. Keep the prior
shell and package receipts. Rollback restores that shell/package; it never deletes
`native-online` saves or recovery session copies. Source/revision mismatch requires
reviewed migration, not deleting player progress. See
[the runtime/save contract](../../../docs/emulators/flycast-dcnet.md).
