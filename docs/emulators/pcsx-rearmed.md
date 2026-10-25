# PCSX ReARMed multiplayer preparation

## Raw images and descriptors — 25 October 2026

По `cdriso.c` ревизии `c8816799b50388e61cfe237fe2cdbb7d8175f20a`
добавлены BIN/IMG/MDF/EXE и CCD/TOC/MDS в `RetroArchNetplay.cpp`.
В хеш входят metadata, все одноимённые native fallback candidates и CUE tracks.
TOC допускается только с одним одноимённым backing image, как у этого reader.
Перед запуском metadata разрешается в native data path; M3U переписывается
в приватный список абсолютных путей. Личные настройки/карты не меняются.
Полный `retroarch` suite прошёл, включая изменения/утрату зависимостей и запуск.
CBN/CBIN пока исключены: `handlecbin` читает count индексных элементов,
но последний блок использует также незаполненный элемент count. Нужен native fix.
[Общая таблица и обновление/откат](../MULTIPLAYER_CONTENT_FORMATS.md).
Core не пересобирался, нужна новая оболочка; прежний запрет CCD/TOC/MDS ниже заменён.

## PBP, M3U and ISO admission — 25 October 2026

S02 registration → S03 identity/preparation → S06 Lobby now accepts PS1 PBP,
M3U and ISO in source, alongside existing CUE/CHD. No title whitelist or new
transport is introduced. DuckStation's ordinary registration also accepts ISO;
existing emulator/save bindings are not rewritten. The network session still
routes a copy of the registration to PCSX ReARMed.

The bounded PBP preflight follows `handlepbp` in the reviewed
[cdriso.c](https://github.com/libretro/pcsx_rearmed/blob/c8816799b50388e61cfe237fe2cdbb7d8175f20a/libpcsxcore/cdriso.c):
PBP offsets, PSISOIMG00 / PSTITLEIMG signatures, up to eight embedded images,
TOC count/timestamps and block-table bounds. It reads at most roughly 1 MiB
of index data per disc, then fingerprints the entire container (maximum 8 GiB).
It does not decode or certify every compressed sector. PSP PBP and encrypted or
unrecognized payloads do not become PS1 content merely by their extension.
Multi-disc `_1.sbi` … `_8.sbi` companions are included as applicable, as are the
existing SBI/SUB and BIN/IMG dependencies. Changing a later embedded disc or
sidecar changes the fingerprint.

ISO admission recognizes the native 2048-byte sector heuristic or raw 2352-byte
sectors and requires a primary ISO9660 descriptor. It retains the 2 GiB disc
bound. Unsupported same-basename TOC/CCD/MDS or overriding CUE descriptors remain
excluded until their dependencies are integrated. Existing CUE/CHD identities
are preserved; this is not content-normalized matching across ISO/CHD/PBP.

M3U follows the native [eight-slot parser](https://github.com/libretro/pcsx_rearmed/blob/c8816799b50388e61cfe237fe2cdbb7d8175f20a/frontend/libretro.c),
with a 64 KiB manifest bound and lines up to 1022 bytes. Relative paths (including
parent directories) and Linux absolute paths resolve to local media. Order matters;
comments, CRLF and the library's directory names do not affect the identity.
Whitespace is not silently trimmed. Nested M3U, oversized lists, quoted paths,
NUL/invalid UTF-8 and foreign drive/backslash syntax are not admitted. Multi-disc
PBP inside M3U is excluded because this native path only exposes its first image;
the PBP itself can be selected directly. Each admitted disc's full identity is
checked, including CUE tracks and sidecars. No playlist or ROM is rewritten.

Preparation revalidates without the scan digest cache, disables remembered initial
disc selection for the private PS1 session and launches the original path through
the existing RetroArch host/guest mechanism. Personal cards remain unattached;
ordinary settings and history are preserved. No new disc-switch UI or synchronized
mid-session disc-switch contract is claimed.

This is source integration, not installation on Flip/Odin or a new emulator build.
No handheld was contacted. Full retail decoding and gameplay are not established
by the structural fixtures. CCD/TOC/MDS, additional formats and ordinary save-card
migration remain separate unfinished work.

Windows checks: RetroArchTests 73 passed, zero failed, one existing PS2 platform
skip; the final focused PS1 run passed 8/8 after adding late-PBP-payload mutation.
StandaloneTests passed 55, zero failed, six existing Linux-only skips, including
literal child-process launch/return for ISO/PBP/M3U. Its obsolete GBA ZIP rejection
expectation was updated to reflect the preceding archive increment; RAR remains
rejected. `runtime_multiplayer` passed in 120.91 seconds. The Windows application
build and adapter export/knowledge validation passed (209 runtime source files).
No ARM64 shell build or device delivery was performed in this increment.

## Ordinary Hotseat delivery — 12 October 2026

Flip and Odin now offer **Separate controllers** (the default) or **Take turns
with one controller** in the ordinary Play together flow, including an already
running game. Invitations disclose the selected mode and fresh temporary progress.
The existing save/leave barrier remains before replacing a solo game. Home shows
the current owner or **Pass controller to [friend]**; only that owner can pass.
Consumed actions return focus to Continue; old party/epoch buttons cannot transfer
another turn. Local Home and touch remain available to the spectator.

This two-person PS1 profile uses stock RetroArch, not an emulator patch. Both
participants request only P1, the guest starts as a spectator, sharing is disabled,
and P2 is disconnected. The guest's loopback bridge observes native SYNC/MODE
responses. The host coordinates release, confirmed vacancy, acquisition and
confirmed ownership through the accepted GameParty. Missing/refused confirmation,
unexpected ownership or party loss ends the owned temporary session rather than
blindly retrying a toggle. Both native processes close on ordinary peer Exit.

Actual ordinary **Nearby** invitation/consent/launch completed three handoffs and
two-player Stroke gameplay in Hot Shots Golf 2 US: Flip hit Sam's ball and Odin
hit Mel's. Guest Exit closed both copies. Actual **Online friend** then used the
public RetroArch tunnel (native connection confirmation, about 149–151 ms), with
three confirmed handoffs, both players' strokes and host Exit closing both copies.
These are two handhelds on the same home network using a remote relay, with
injected pad input. Physical controls, different internet connections, sustained
play and wider game compatibility remain acceptance targets.

An earlier online return handoff exposed ordering between native MODE and the
provider's phase message. A regression reproduces it; the final coordinator accepts
an already confirmed remote acquisition without issuing another toggle. Both
message orders and duplicate phases pass. A lifecycle test also exposed and fixed
destructor callbacks after RuntimeMultiplayer state teardown. Tests preserve the
ordinary save barrier, cancel owned preparation, reject stale actions and verify
native-role commands and bilateral exit.

The five focused Windows suites passed (47.72 s); native ARM64 component checks
and the full runtime suite passed. After the final ordering change, the handoff
and runtime suites passed again on ARM64 (2/2, 2.24 s) and Windows (2/2, 3.06 s);
both application builds succeeded. The portable runtime export contains 123 checked files.

Installed TrainerOS SHA-256 on both devices:
`f942597eb18e0b08e93744c3f89d6cbf1c43f33abf30da3aa83e0d2919452034`.
Admission requires the reviewed RetroArch executable
`8d57b47c88d416c05145187c8c906b7ab0013edb7e45af69d9b6bb841aea4a24`
and core `25bfeb6336d7ef573903775fe53733af974c90edd268cf4fb9afa497e188609d`.
The native observer is pinned to protocol 5–7 at RetroArch `69a4f0ea1e`;
updates require rechecking framing, exclusive ownership and ordinary handoff.
Separate-controller profiles retain their existing admission.

Final post-exit checks: all 364 Flip / 25 Odin baseline save files unchanged,
ordinary RetroArch configurations unchanged, both databases `quick_check=ok`,
no remaining RetroArch processes and volume zero. The ordinary invitation tests
successfully wrote a private temporary card; no personal card import/return or
persistent multiplayer progress is claimed. ROM/BIOS/private captures stay outside
Git. PS2/GameCube Hotseat, the complete mode catalogue and the retained external
acceptance remain open. No issue or whole multiplayer block is closed.

Implementation: [coordinator](../../src/features/adventure/HotseatHandoff.cpp),
[runtime binding](../../src/features/adventure/RuntimeHotseat.cpp),
[observer](../../src/integrations/adventure/retroarch/RetroArchHotseatObserver.cpp)
and [session preparation](../../src/integrations/adventure/retroarch/RetroArchNetplay.cpp).
Rollback the TrainerOS binary to remove this mode; no emulator replacement or
ordinary card/configuration rollback is needed.

## Native shared-P1 continuation — 12 October 2026

Historical isolated probe below; the ordinary delivery above supersedes its
not-connected/not-installed status and retains its narrower evidence.

An isolated Flip/Odin Hot Shots Golf 2 US session reached Stroke mode with two
in-game players both assigned controller 1. Flip made Sam's first stroke; after
native P1 transfer Odin made Mel's first stroke. Five confirmed ownership changes
used unmodified RetroArch `NETPLAY_GAME_WATCH` and its existing exclusive port
assignment. Input from the spectator did not activate the game menu, whereas the
current owner could activate it. This used injected virtual-pad input, not owner
physical acceptance. It is not the finished Game Options flow.

The guest's loopback observer recorded native `MODE` packets: each release had
zero devices and each acquisition had only bit 0 (P1). Host/guest native client
IDs were 0/1. Successful acquisition frames were 4954, 8321, 11941, 17582 and
20212. One command during loading did not release P1; the host's subsequent
request was refused. This demonstrates why receipt of a local toggle command is
not handoff confirmation. The maintained coordinator waits for native release
before requesting acquisition, rejects stale/duplicate control messages, and
fails on missing acknowledgement rather than retrying an uncertain toggle.

The new observer reads the pinned native
[SYNC/MODE layout](https://github.com/libretro/RetroArch/blob/69a4f0ea1e/network/netplay/netplay_frontend.c)
without changing input packets. Its retained data excludes nicknames and SRAM.
GameParty routes bounded control packets only between accepted running members,
bound to current party, peer and boot. The current implementation is still being
connected to mode selection, Game Options, launch and exit. No Hotseat profile is
enabled or installed by these development checks.

Focused `netplay_client`, `hotseat_handoff` and `game_party` checks pass on Windows
(3/3, 5.10 s) and ARM64 (3/3, 1.48 s), including fragmented native packets,
exclusive ownership, refused/late/duplicate handoffs, party/boot binding and
preventing buffered events from leaking into a replacement session. These are
development checks, not ordinary invitation or installed Hotseat acceptance.

Both private emulator processes exited normally. Ordinary RetroArch config hashes
and the bounded ordinary-save baseline (364 Flip / 25 Odin files) are unchanged;
both databases pass `quick_check`, both shells remain alive and volume stays zero.
The game reported an unsuccessful initial private-card save; cancelling that
prompt allowed play. No personal card was attached or replaced, and persistent
progress is not claimed. Private probe material stays outside Git.

Probe fingerprints: core
`25bfeb6336d7ef573903775fe53733af974c90edd268cf4fb9afa497e188609d`,
CHD `34f29333c3c9762463d4b77e3611dd9dee5871b32ed76daafedeb550e69d7215`,
owned BIOS `11052b6499e466bbf0a709b1f9cb6834a9418e66680387912451e971cf8a1fef`.
Hotseat UI/delivery, internet handoff, GameCube/PS2, the complete mode catalogue
and retained physical/distinct-network acceptance remain open.

## Original two-pad preparation

11 October 2026. Reuse the unmodified libretro core with TrainerOS's existing
RetroArch direct/relay networking. This adds a PS1 shared-console route, not
DuckStation netplay or a PlayStation link-cable implementation.

## Source and installation

Inspected upstream: [`c8816799b50388e61cfe237fe2cdbb7d8175f20a`](https://github.com/libretro/pcsx_rearmed/tree/c8816799b50388e61cfe237fe2cdbb7d8175f20a).
The core's [official documentation](https://docs.libretro.com/library/pcsx_rearmed/)
lists netplay; core-info revision `5a74858ab2f7a50cebb5a6330895bc38899531c0`
declares deterministic states. This research pin is not a claim that the existing
binary was compiled from today's source.

The previously recorded ARM64 bundle contains `pcsx_rearmed_libretro.so`,
1,747,256 bytes, SHA-256
`25bfeb6336d7ef573903775fe53733af974c90edd268cf4fb9afa497e188609d`.
See [baseline](../../packaging/emulators/arm64-baseline.json). Devices are off;
that historical installation has not been freshly read. No core/package is
changed by this pass. Discovery and launch compare actual current core/runtime
hashes on each participant rather than trusting the historical manifest.

## Maintained session configuration

[RetroArchNetplay.cpp](../../src/integrations/adventure/retroarch/RetroArchNetplay.cpp)
owns the reproducible integration. It uses two digital pads, disables Multitap,
and selects a private owned BIOS directory. Session-only core options are
`pcsx_rearmed_bios=auto`, `pcsx_rearmed_memcard1=libretro`,
`pcsx_rearmed_memcard2=none`, `pcsx_rearmed_multitap=disabled`, the earlier
`pcsx_rearmed_multitap1/2=disabled` keys and
`pcsx_rearmed_gpu_thread_rendering=disabled` (corrected on 23 October 2026;
the earlier key `pcsx_rearmed_threaded_rendering` was ignored by the installed core).

RetroArch uses `--sram-mode noload-nosave` and a private save/state directory.
Even cores predating the newer card options cannot reach the ordinary card
directory through this configuration. No personal card import/return is added.
The session copies only the selected BIOS; no BIOS is shipped or exchanged.
Discs stay read-only in their original location. Full content/track and relevant
sidecar hashes are rechecked before launch; the private BIOS copy is rechecked too.
[Format boundaries](https://github.com/EriArk/TrainerOS/blob/c6aaac25ba4f55262f3f6418e8d29cac7cdc9aec/docs/MULTIPLAYER_PREPARATION.md#ps1-runtime-decision--11-october-2026)
remain explicit. Ordinary core options, saves and adapter bindings are unchanged.

## Checks, update and rollback

23 October 2026 follow-up: the installed runtime/core hashes above were freshly
verified on Flip/Odin. CTR US completed native P2 join, graceful guest exit and
P2 rejoin while the host PID remained unchanged, with private session progress
and unchanged personal configuration/save baselines. TrainerOS now admits this
reviewed local core to the shared **Rejoin game** action; returning to Lobby or
restoring signaling never launches the guest automatically. No core patch or
ordinary memory-card migration is involved. See the current
[return contract and bounded evidence](../LOBBY_LIVE_JOIN.md).

Local fixture checks cover BIOS/content/sidecar changes, private directory cleanup,
separate controller ports, unsupported descriptor rejection and unchanged ordinary
cards/settings. See the dated preparation register for final build/test results.
Fixtures do not prove a valid retail disc, real core behavior or paired gameplay.
Handheld installation, actual controls, games and separate internet remain pending.

For a core update, review upstream BIOS/card/disc options, align actual peer builds,
rerun affected local checks and the deferred bounded paired game/exit check before
recording installation acceptance. There is no emulator patch to rebase. Rollback
removes this TrainerOS profile or returns to the previous client; no ordinary
memory-card format migration or downgrade is needed.
