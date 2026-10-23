# PPSSPP maintenance

## Silent coordination loss — 24 October 2026

The follow-up closes the missing Linux timeout on the coordination TCP socket,
using the same 15-second `TCP_USER_TIMEOUT` already applied to relay sockets.
PPSSPP sends its existing ping every two seconds. Previously the coordination
socket only enabled keepalive (180-second idle, 60-second interval, 20 probes)
and left outstanding data subject to system TCP retry policy.

[The patch](../../tools/ppsspp-relay/coordination-timeout.patch) changes only that
socket's maximum unacknowledged/zero-window wait. It does not set a receive-idle
deadline: the server consumes `OPCODE_PING` without an application reply, so
ordinary quiet lobbies must remain connected. Linux returns `ETIMEDOUT`, which
the preceding coordination-stream repair handles through existing cleanup.
The deadline starts with outstanding data; detection also includes the ping
interval and kernel scheduling. [Linux TCP contract](https://man7.org/linux/man-pages/man7/tcp.7.html).
Failure to set the option is logged. Other platforms retain their existing behavior;
this delivery is Linux ARM64, not a new Windows/Android package.

[Actual-source fixture](../../tools/ppsspp-relay/coordination-timeout-test.py)
compiles the socket setup, keepalive, ping, receive and cleanup blocks from the
built PPSSPP source. It uses real TCP and packet dropping in disposable Linux
network namespaces, refuses the host network namespace, and contacts no public
service. UI/logging/peer-timeout notification are stubbed. Results on Odin:

- Prior source: after 18.118 seconds of packet loss, still marked connected.
- Patched source: timeout and cleanup after 17.196 seconds, exactly one peer
  timeout notification, invalid socket and empty receive buffer.
- Three-second loss followed by 18 seconds of recovered connectivity: no disconnect.
- Eighteen seconds without application replies, with normal TCP ACKs: no disconnect.
- Fresh connections after each patched case received clean data.

`build.sh` compiles the fixture. Its header documents explicit isolated-namespace
execution (`persistent`, `brief`, `quiet`); it does not alter host firewall rules.
The earlier receive/stream/retry and 15000-ms production I/O checks passed again
because the emulator recipe changed. The ARM64 shell build and `standalone` suite
passed (6.29 seconds); unchanged broader lifecycle suites retain the previous
delivery's evidence. Adapter export/knowledge checks passed.

Installed PPSSPP SHA-256 on both Flip/Odin:
`dec19c9c42380155cde9c492f1d998ea6d13701b592d331d4f0073018cee73e6`.
Admission adds `-coordination-timeout-v4` and retains earlier identities for
rollback. S03/S07 ownership, S06 explicit launch and native game retry remain
unchanged. There is no automatic group rejoin, emulator restart or launch from chat.

Flatpak commit: `77de45a6cdf4f29f447a2f74814ce2cc48c8d8d62e826d2d5861c635ecb36bbc`;
bundle SHA-256: `ac84c7c970dc7bd277f904417735011ae1e5837f504d7bf820106879d865021d`.
Installed/live shell SHA-256:
`ee363389b6056d0a730606f3d2292494fcbd7cbbc3fd6752ea1bdc6e5be8a510`.
Both live PPSSPP processes matched the new hash and rendered Tekken 6 startup
using copied configuration/SAVEDATA, then closed normally. Original personal
file hashes were unchanged; shell QML errors were zero and device volume stayed
zero. This adds installed startup/exit evidence to the isolated socket checks,
not a new paired-match or native game rejoin result. Stable and earlier bundles
remain available for rollback; no system-wide TCP/firewall settings changed.
Rebase by applying both coordination patches after the relay patches and rerunning
the namespace cases before admitting the new executable hash.

This completes the current relay/coordination repair increment. Wider PSP game
compatibility and private admission remain in R037; the owner's next selected
implementation is [PS2 native LAN/Online](../PS2_NATIVE_MULTIPLAYER.md).

## Coordination stream recovery — 24 October 2026

The follow-up addresses the separate coordination TCP connection in PPSSPP
1.20.4 `friendFinder`, retaining the same upstream and relay pins. Owners remain
S03/S07 through F04/F12; S06 launch intent and native PSP matchmaking are unchanged.

The original receive block processes positive reads but ignores EOF and fatal
receive errors. A server can therefore close its connection while PPSSPP still
considers it connected, until a later ping/send fails. Its partial receive buffer
also survives into the next TCP connection. Neither symptom establishes that all
reported Tekken 6 failures have this cause.

[The bounded patch](../../tools/ppsspp-relay/coordination-stream.patch) handles EOF
and fatal receive errors through the existing disconnect/peer-timeout policy,
clears partial bytes on disconnect and before a new login, and retains temporary
EAGAIN/EWOULDBLOCK/EINTR reads. A full buffer is parsed before another read, avoiding
a zero-length read being mistaken for EOF. It does not reconnect to a game group,
restart the emulator or launch from chat/presence; native game actions still own
rejoining. Silent packet loss without EOF/error remains a different failure mode.

[The regression fixture](../../tools/ppsspp-relay/coordination-stream-test.py)
compiles the actual patched FriendFinder receive/cleanup blocks with real local
TCP sockets; UI/logging and peer-timeout notifications are stubbed. The unpatched
baseline reproduces EOF leaving the connection and partial bytes alive. The
repaired block covers partial data, transient errors, full receive buffer, EOF,
TCP reset, clean data after reconnect and 20 repeated disconnect/reconnect cycles.
This is socket-level evidence, not a native game rejoin claim.

Build/package recipes apply and retain the patch and fixture with provenance.
The earlier receive, retry and production-timeout fixtures also passed in the
ARM64 build. TrainerOS `standalone` and `runtime_multiplayer` suites passed in
116.60 seconds; adapter export/knowledge checks passed.

Installed on both Flip/Odin, with the stable Flatpak retained:

- PPSSPP SHA-256: `a8489753f9691ab00bd5e7fd1e7c392bd10d3119635e44c598896d76a8ebc5f2`.
- Flatpak commit: `29350de9f7d761cbfc6feb1f21d330f235e8bb47582b7cd09a1ef8a0f562e2f3`.
- Bundle SHA-256: `02c86bff6dfa1bbc7532dccc6bee0d747be8374eded7d556c4ce958bdbbe0c77`.
- TrainerOS SHA-256: `0b37538532e2646e2e55115c0da6fe43ce70248f7a0c2effd6384e4300edc779`.

Exact admission adds `-coordination-recovery-v3`; earlier hashes remain available
for rollback and mixed unverified revisions remain separated. Both running shells
matched the installed hash with zero QML errors. Both installed PPSSPP processes
matched their hash and rendered Tekken 6 using copied configuration/SAVEDATA.
They closed normally; original SYSTEM/SAVEDATA hashes were unchanged and volume
remained zero. The diagnostic native-menu traversal did not reach a coordination
login, so it adds startup/exit evidence only, not a new native reconnect result.
The temporary loopback listeners were stopped; no service was installed.

Rollback uses the retained stable branch or the previous recovery bundle together
with an admitted shell. To rebase, apply the patch to the pinned PPSSPP source,
rerun the extracted-source TCP fixture and ordinary relay checks, then rebuild and
record the new executable identity. Do not transplant whole networking rewrites
merely to carry this receive correction. Silent coordination loss without an
EOF/error and native game retry behavior remain distinct follow-ups.

## Long-outage recovery — 23 October 2026

Follow-up to the receive repair below; ownership remains S03/S07 through F04/F12,
with S06 preserving deliberate launch intent. This changes the existing Ad Hoc
transport, not native game matchmaking or automatic PSP relaunch.

Two additional failures were localized:

- PPSSPP 1.20.4 permanently sets `relayDisabled` after 15 seconds of repeated
  relay failures. Only Ad Hoc initialization clears it. On the previously delivered
  `6db5a5bc…` pair, stopping the isolated relay produced 863/924 failed PDP creates
  within 15 seconds; after relay restart neither process opened a new relay TCP
  connection. This is a reproduced recovery defect, not an inferred timing tweak.
- The pinned library's blocking handshake and partially completed sends/reads
  have no deadline. A real loopback TCP server that accepted but withheld its
  handshake kept the old call blocked until the server closed at 900 ms. With
  the fixture's 300 ms deadline the repaired call returned at 301 ms, released
  its slot and connected successfully on a subsequent attempt.

[Retry patch](../../tools/ppsspp-relay/relay-retry.patch) replaces permanent
disable with a shared retry cooldown: one second initially, five seconds after
15 seconds of failure. Existing handles are usable during cooldown; allocating
an emulated socket does not fail merely because another socket needs recovery.
Success/new Ad Hoc initialization resets the cooldown. Retries prepare transport
only; the player still chooses a native challenge/join action.

[Socket patch](../../tools/ppsspp-relay/socket-timeout.patch) bounds unfinished
blocking wire operations to 15 seconds and waits using `poll`, replacing the
busy retry loop. A partially transmitted/received frame fails as a connection
error; it is never retried as a new frame on the same stream. Existing close and
slot cleanup paths own recovery. Linux relay sockets also set `TCP_USER_TIMEOUT`
to 15000 ms for unacknowledged/zero-window data; this is per-socket, not a system
TCP policy change. It does not time out an otherwise idle connection.
[Linux socket contract](https://man7.org/linux/man-pages/man7/tcp.7.html).

The pinned TCP protocol and PPSSPP version remain unchanged. The reviewed
[current upstream PPSSPP source](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceNetAdhoc.cpp)
still contains the permanent-disable path; newer aemu client work also introduces
ENet and API changes. These bounded patches avoid importing that wider migration.
They do not make a disconnected match continue uninterrupted: after a long loss,
the game may report failure and require a new native challenge.

### Regression and delivery

The build runs the earlier PDP/PTP receive fixture, the real TCP
[stall/reconnect fixture](../../tools/ppsspp-relay/relay-timeout-test.c), and
[retry policy checks](../../tools/ppsspp-relay/relay-retry-test.cpp) against the
header actually applied to PPSSPP. Coverage includes repeated failed handshakes
followed by success, full send buffers, immediate nonblocking refusal, partial
receive timeout, cooperative abort, the old 15-second boundary and cooldown reset.
The production timeout was also measured without the fixture override: 15000 ms.
`getsockopt` confirms the Linux user timeout on a real created relay socket.

Installed ARM64 binary SHA-256:
`b5acbf94ee69f270b55759cc84e22d4ee54798e9e0eb6d438d2ae87cbe5c9906`.
Settings append `-relay-recovery-v2`, separating unverified mixed old/new pairs;
prior exact hashes remain admitted for rollback. Package commit:
`21749139b4a4a69726c2f49defec833f1494791ced9b8da9833d0cc98fb2b926`;
bundle SHA-256:
`2fd2c7dbbba0ebfcf83155ee539879da6c2b676cd8001172fc751f82850ad79b`.

On both handhelds the repaired build kept the same game processes through an
isolated relay outage longer than 60 seconds. Each recorded 25 failed PDP creates
over about 61 seconds, following the one/five-second cooldown instead of the
baseline's hundreds of requests followed by permanent disable. After relay
restart both re-established two TCP connections, rediscovered the opponent in
Tekken 6's native Battle Lobby, and loaded a new fight through challenge/accept.
The round timer advanced; this is recovery and loading evidence, not a new full
match or independent physical-input claim. The temporary relay was then stopped.
This forced local-relay case does not establish WAN or silent packet-blackhole
recovery; the latter's per-socket timeout is covered by configuration/fixture checks.

The package is installed as the current `traineros-relay` branch on Flip/Odin;
the original stable package remains available for rollback. Both default package
resolution and a live installed game process matched the binary hash above.
Freedesktop Platform packages were retained. Shell SHA-256
`dfd6e1179c775c6b77e8c48255acdb8c22c02ad9ab3f7ebc4e1b9c1a30c424de`
is installed and running on both; live hashes match, QML errors are zero.
The five ARM64 suites (`standalone`, `lobby`, `game_party`, `runtime_multiplayer`,
`social`) passed in 118.39 seconds; adapter export/knowledge checks also passed.
Diagnostics used copied private settings/saves. Both games closed normally;
original SYSTEM/SAVEDATA hashes were unchanged and device output remains zero.
The receive-only builds below are retained historical evidence, superseded on
both devices by this recovery build. Coordination-server loss remains a separate
failure path from the relay outage tested here.

## Relay receive repair — 23 October 2026

The reported Tekken 6 loading/disconnect problem is being investigated separately
from the earlier Lumines acceptance. The reproducible defect is in the relay
client shipped with PPSSPP 1.20.4, not a demonstrated device-specific clock fix.
Remote Play and automatic PSP relaunch are outside this change. TrainerOS prepares
the Ad Hoc transport; native game controls still own finding/rejoining a match.

### Source and bounded change

- PPSSPP stays on tag `v1.20.4`, commit
  `fa50bb1976065c4f8b1b47af227d367fe9771555`.
- Replace its aemu_postoffice pin `530fee545c27ffb8524a8f496cbbcfdb687fe8c5`
  with maintained upstream commit `572de778fabcbda819ab0f677368c3fbc3033e19`.
  This brings the upstream PDP/PTP receive ring buffers, EOF detection and
  `pdp_buffered_data_size`; it does not introduce a TrainerOS network protocol.
- [Integration patch](../../tools/ppsspp-relay/pdp-stat.patch) makes PDP status
  report complete queued payload bytes, caps the result to the emulated socket
  buffer and clears a dead handle. Poll receive readiness now consults buffered
  PDP/PTP data, because a drained OS socket may still have readable library data.
  Native direct/LAN socket handling is unchanged.
- Session-only `PauseOnLostFocus=False` and `PauseWhenMinimized=False` prevent
  ordinary solo preferences from stopping the emulated peer on Home/chat/minimize.
  Generic settings identity is `ppsspp-adhoc-native-clock-focus-v2`; exact Lumines
  uses `ppsspp-adhoc-isolated-config-clock-focus-v3`. The repaired binary adds
  `-relay-recv-v1` to either identity, keeping an unverified mixed old/new pair
  separate while retaining the original build for rollback. Global/per-game originals,
  controller bindings, unique MAC and saves remain owned by each device.
- Generic `ForceLagSync2=False` and exact Lumines `True` remain unchanged. Tekken
  completed one two-round Online match in each clock mode; that comparison does
  **not** establish a clock cause or justify a new universal timing override.

Owners remain S03 (runtime/session/save), S06 (Lobby intent) and S07 (connection),
through existing F04/F12. No new manager, automatic launch trigger or PSP Rejoin
policy is introduced.

TrainerOS ARM64 build and the five `standalone`, `lobby`, `game_party`,
`runtime_multiplayer`, `social` suites passed after the final discovery change.
Tests cover session-only pause overrides/original config preservation, current
Flatpak branch selection with rollback and user-installation precedence.
Shell SHA-256 `b0fdbd39f234b8649ab81468c6b4ef15ea8bbe72d0d557caf6e90e0a76968b64`
is installed and running on both Flip/Odin; live hashes match and QML errors are
zero. Adapter export/knowledge checks passed. Wi-Fi power save remains off and
device output volume remains zero. The shell's change selects the installed
branch; it does not automatically launch or rejoin a PSP game.

### Reproduction and validation

[Local stream-socket fixture](../../tools/ppsspp-relay/relay-receive-test.c) uses
no public service or game data. Built against the exact old library with
`-DLEGACY_RELAY`, it reproduces EOF as WOULD_BLOCK (-4) with the handle still alive,
and a nonblocking receive that waits for the delayed remainder of a packet.
The old status path reports 72 bytes for queued 72 + 120 byte messages.

Against the new pin it verifies DEAD (-3) on EOF, WOULD_BLOCK for a partial
message, intact later receipt, all 192 queued bytes, repeated ring wrap, retained
PTP data across short reads and PTP EOF. It also confirms that an empty OS read
set can coexist with complete data in the library, requiring the poll integration
above. The earlier timed probe measured roughly 300 ms waiting in the old read
versus immediate return in the new read; these are defect reproduction results,
not game latency benchmarks.

Unmodified-game diagnostics used **copies** of both devices' configuration and
SAVEDATA. Tekken 6 ULUS10466 completed a Nearby match with a win/loss result, and
Online two-round draw matches with clock synchronization both on and off. One
earlier Online run disconnected; a separate Flip `VK_ERROR_DEVICE_LOST` incident
is a graphics failure, not evidence of a relay fault. Rematch was not established.
Original SYSTEM/SAVEDATA hashes remained unchanged. The socket defects are proven;
a causal link to every reported Tekken disconnect is not yet proven.

The repaired ARM64 pair completed a full Tekken 6 match with win/loss results
through an isolated, forced **local** aemu relay. After stopping/restarting that
relay in the native opponent lobby, both existing game processes rediscovered
each other; a new native challenge loaded an arena and the round timer progressed.
No TrainerOS Rejoin or emulator restart was involved. This is same-network relay
and native recovery evidence, not different-network reliability certification.
The temporary relay was stopped afterwards; no permanent service was installed.
Diagnostic games used copied saves, and original SYSTEM/SAVEDATA hashes matched
after both processes closed. Rematch-menu timing remains unverified.

### Build and package

[Build recipe](../../tools/ppsspp-relay/build.sh) uses the same Freedesktop SDK
`aarch64/25.08` as the installed Flatpak generation, pinned submodules, the patch
and the regression fixture. It builds the SDL executable without replacing the
installed application. The ARM64 executable built successfully with SHA-256
`6db5a5bc7263b930b29fa6a65348c8a292acdb38306b74bdee071a22c5339834`.
Dynamic dependencies and relocations resolve in the installed Flatpak runtime
on both devices. The repaired package is installed and selected on Flip/Odin;
default `flatpak run` resolves the exact executable above. The system `stable`
branch remains installed for rollback. Bundle SHA-256:
`f3eb92200499e1639e7f4d440c58c5e506d5a243e10846384bb40626a1a03700`;
OSTree application commit:
`8fcacc9a8f8578588f376c8503a22b72266f73f12ce34bab326e731a92f87024`.
Odin's first install also updated its 25.08 Platform/extensions; Flip's install
used `--no-deps --no-related` to retain its existing runtime. Dependency/relocation
checks passed under both installed runtime revisions. These are separate from
the pre-install candidate gameplay checks above. Both installed binaries also
rendered the Tekken startup screen with matching live executable hashes. One Odin
diagnostic exit took about 53 seconds while its opt-in debugger waited for HTTP
startup; it then exited normally. This diagnostic-only delay is not counted as
an Ad Hoc disconnect or hidden by force-killing the process. Original files
matched after exit.

[Package recipe](../../tools/ppsspp-relay/package.py) creates a **local**
`org.ppsspp.PPSSPP/aarch64/traineros-relay` branch. It verifies the exact old
binary hash, retains the base package's SDL2/libraries/assets/licences, replaces
only PPSSPPSDL and includes patch/build provenance. It does not install the bundle.
This is not a public Flatpak release or an x86/Steam Deck build.

Only the reviewed executable hash is admitted. Discovery now resolves Flatpak's
`current/active` selection when `info APP` fails because both branches are present;
the user installation takes precedence over the default system installation.
Rollback stops the owned game through normal exit and selects
`flatpak make-current --system org.ppsspp.PPSSPP stable` in the same
installation; do not restore old personal saves. Rebase by checking both upstream
pins and APIs, reapplying the bounded patch and rerunning the socket fixture before
candidate-game validation. A branch switch does not certify universal PSP support.

**Owner decision, 12 October 2026:** installed ordinary Nearby/Online multiplayer
is sufficient for the current milestone. Private-party admission is deferred;
do not resume its prototype or repeat already passed Lumines checks as current
work. Keep wider game/format compatibility separate from the platform-wide route.

## Updated-handheld recheck — 11 October 2026

The current ARM64 client is installed on both updated devices. The exact Lumines
Nearby invitation, host save/leave barrier, native discovery and paired arena
were rechecked with independent input. The first challenge returned to the lobby;
the second reached gameplay. Home/Exit cleaned both temporary configuration roots;
ordinary PSP save hashes remained unchanged. A real generic UMD parser check is
not wider-title gameplay evidence. [Full evidence and limits](https://github.com/EriArk/TrainerOS/blob/c6aaac25ba4f55262f3f6418e8d29cac7cdc9aec/docs/MULTIPLAYER_DEVICE_RECHECK_20261011.md).

## Generic Ad Hoc preparation — 11 October 2026

Source preparation extends the existing invitation/relay/launch route beyond the
exact Lumines disc. It uses **unchanged PPSSPP 1.20.4**, tag commit
`fa50bb1976065c4f8b1b47af227d367fe9771555`, retaining the reviewed ARM64 runtime
hash below. This does not prove that every PSP game contains or supports Ad Hoc.
The party surface asks players to choose Ad Hoc inside the game and distinguishes
original online servers. Explicit solo catalogue metadata excludes invitations;
missing metadata is not a single-player classification.

`runtime.ppsspp.adhoc.v1` admits raw ISO UMD games: bounded ISO9660 reads locate
`PSP_GAME/PARAM.SFO`, require category UG and a disc ID, reject ambiguous entries
and invalid extents, then match the whole ISO SHA-256 and runtime between friends.
IDs establish format, not multiplayer support or semantic save access. Local
titles/filenames are presentation only. CSO/CHD/PBP, UMD video and other formats
remain outside this preparation; ordinary solo launching is unchanged.

This first generic profile prepares two participants. Catalogue player counts
do not increase its capacity; larger parties need reviewed native-mode evidence.
Players use native create/join controls. Automatic game-room selection, private
game lobbies and exclusion of unrelated public-server users are **not** delivered.
Parallel private-party admission and games needing additional setup stay open.
No ROM, save or firmware is transferred.

Exact Lumines retains its existing wire identity and clock override. Generic
discs use native-default `ForceLagSync2=False` and a distinct settings fingerprint.
Both reuse private SYSTEM copies, controller bindings, saved MAC, ordinary
SAVEDATA and cleanup. PPSSPP can write its ordinary saves during play; this is not
temporary shared-console progress or TrainerOS semantic save editing.

Sources checked:
[Ad Hoc/relay](https://www.ppsspp.org/docs/multiplayer/how-to-play/),
[quickstart](https://www.ppsspp.org/docs/multiplayer/quickstart/),
[ISO structure](https://github.com/hrydgard/ppsspp/blob/fa50bb1976065c4f8b1b47af227d367fe9771555/Core/FileSystems/ISOFileSystem.cpp),
[SFO structure](https://github.com/hrydgard/ppsspp/blob/fa50bb1976065c4f8b1b47af227d367fe9771555/Core/ELF/ParamSFO.cpp).
The bounded reader is TrainerOS code, not a copied emulator filesystem.

Local validation: Windows client and scoped targets build successfully;
`standalone`, `game_party`, `runtime_multiplayer` pass **3/3**, 7.01 seconds.
The three new raw-disc/clock/lifecycle regressions also have explicit QtTest file
logs (none skipped), because this Windows CTest run captured no console output.
Linux memory-stick symlink cases remain platform-skipped on Windows; no new Linux
save-isolation result is claimed. Adapter exports/registry and local doc links
validate. Final Windows debug client SHA-256:
`3955ba7105f732df27e80a1e875ab13bd44ebaacbec3115ac50292a0ff3b9b94`.
Ninja recovered a truncated dependency cache and performed a full rebuild; only
the final build above includes the complete source changes.

Handheld installation,
wider-game discovery/play/controls, four clients, loss/reinvite and distinct
networks remain deferred. Earlier Lumines evidence is not promoted to new titles.
No public relay or handheld was contacted. Rollback removes the generic profile
while retaining Lumines; no emulator migration or persistent settings replacement.

## Earlier installed evidence

Updated 2026-10-03. Actual multiplayer acceptance is tracked in
[PSP multiplayer](../PSP_MULTIPLAYER.md); integrated invitations, short LAN and
external-relay rounds, and normal Home exit/cleanup passed on both devices.
The LAN challenge required one retry. A 12-second relay loss during rematch startup
returned both games to the lobby; a fresh challenge restored play without restart.
Distinct-network, mid-round loss and sustained reliability remain unproven.

## Upstream and observed installation

- Upstream: [hrydgard/ppsspp](https://github.com/hrydgard/ppsspp), tag `v1.20.4`.
- Observed package: Flatpak `org.ppsspp.PPSSPP`, ARM64 `PPSSPPSDL`, Flip 2 and
  Odin 2. Runtime SHA-256:
  `c99cec693067f8c94ae0d8b5e7299392ed5c0cfae3f8231176ed25ea6c155dfc`.
- This is the observed netplay allowlist, not a reproducible image package lock.
  This historical unpatched installation was superseded by the 23 October relay
  repair above; the original stable branch remains available for rollback.

## Maintained integration

Source: [PpssppNetplay.cpp](../../src/integrations/adventure/standalone/PpssppNetplay.cpp),
[StandaloneAdapter](../../src/integrations/adventure/standalone/StandaloneAdapter.cpp),
[discovery](../../src/platform/emulation/EmulatorDiscovery.cpp).
The exported runtime copy lives in [adapter implementations](../adapters/implementations/runtime/README.md).

| Change | Reason and ownership |
|---|---|
| Flatpak `--command=env` with `XDG_CONFIG_HOME` set inside the sandbox | Flatpak resets the reserved variable when passed as an ordinary environment override. Native launch uses `/usr/bin/env`. |
| Temporary configuration root inside ordinary `PSP/SYSTEM` | Copy global/per-game/controller INIs because PPSSPP saves appended options. Retain ordinary SAVEDATA and optional GAME/TEXTURES through links; remove only the private root on process settlement. |
| Network `EnableWlan=True`, `EnableUPnP=False`, `PortOffset=10000` | Session settings, not global preference replacement. |
| LAN relay mode `2`, host's actual LAN address, host-only `EnableAdhocServer` | Loopback allowed coordination but prevented real opponent discovery. |
| Online relay mode `1`, `socom.cc`, built-in server off | Exact Lumines pair reached gameplay through external TCP 27312/27313 and recovered via a new challenge after rematch-start loss. Both clients shared home internet; separate-network acceptance remains open. |
| Network savestate/speed control off; General auto-state-load, cheats/plugins off; Achievements disabled | Keep the experimental paired runtime consistent without rewriting the ordinary profile. |
| SystemParam nickname from Trainer; **no MAC override** | Preserve global and per-game console identities. Earlier per-invitation randomization was removed on 2026-10-03. PPSSPP warns that saves can be tied to a MAC. Never put a shared fixed MAC in the image. |
| General `ForceLagSync2=True` in the exact Lumines session; settings fingerprint `ppsspp-adhoc-isolated-config-clock-v2` | Default-clock diagnostic failed after matching; the controlled clock-sync run reached a LAN arena and independent inputs on Flip/Odin. Keep ordinary/per-game preferences intact. This is not a universal PSP compatibility workaround. |
| Flip ordinary graphics backend changed from OpenGL to Vulkan | Observed OpenGL startup crash; private backup retained. Odin already used Vulkan. This is device evidence, not a universal backend policy. |

No socket-timeout or first-connect workaround is enabled. Clock synchronization
uses upstream `ForceLagSync2` in `[General]`, not `[CPU]`. Rollback removes the
session override and restores the previous settings fingerprint together; it
does not rewrite the ordinary INI or saves. Recheck the matching transition on
both devices when updating PPSSPP before removing or retaining this workaround.
Do not silently migrate existing saves to another device/MAC or overwrite a
per-game override to solve an unproven networking hypothesis.

## Update and rollback procedure

1. Record candidate package/commit and runtime hash; compare upstream
   `Core/Config.cpp`, `UI/NativeApp.cpp` and ad hoc implementation against the
   keys and launch assumptions above. If a source patch becomes necessary,
   record its file/build recipe here with the upstream base and rebase result.
2. Test the candidate with copied configuration and copied saves first. Check
   sandbox visibility/write access and `/proc` environment, including that
   the normal memory stick and controller mapping are used by the real route.
3. Run `trainer_standalone_tests`, export/check adapter knowledge, then check
   launch, input, physical Home return, ordinary save readback and cleanup on
   both supported devices. Compare original config/save hashes for diagnostics.
4. For networking, prove an actual match, independent controls, exit and
   interruption/recovery. Online needs separate route evidence. Do not widen
   the exact runtime/content allowlist solely because the build or lobby works.
5. Record the proven candidate and only then change the image/package pin and
   applicable runtime allowlist. Preserve per-game settings, unique device MAC,
   controller preferences and saves through the update.
6. Keep the previous compatible package and pre-migration configuration backup.
   Stop the owned process before rollback; restore only migrated integration
   settings, not old personal saves. A source-patched runtime needs its own
   fingerprint and paired proof before it replaces the installed version.

Relevant primary sources: [network settings and MAC warning](https://www.ppsspp.org/docs/settings/network/),
[v1.20.4 configuration](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/Config.cpp),
[matching implementation](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/HLE/sceNetAdhocMatching.cpp).
