# Nearby play — 2026-09-30

The consoles advertise the active Trainer's name outside the Link screen.
Nearby play now prefers Bluetooth RFCOMM, with ordinary local-network discovery
as an independent alternative. No router is required for the Bluetooth route.
Wi-Fi Direct is retained only as an explicit root-selected experimental route;
its earlier evidence below is historical. Bluetooth failure never starts P2P
scanning. This follows the owner's request after the Odin freeze reports.

An invitation shows the Trainer's name with Accept/Decline over the current
page. Accept establishes a session; it does not select an activity or expose
the Party before acceptance. Either participant can then invite the other to
Battle, Trade, Sell or Give. Each activity has a separate invitation in the
same small popover. Declining an activity keeps the connection. Returning from
an activity or leaving the page also keeps it; X in the connected lobby ends it.
Save confirmations/reservations and interrupted transaction recovery are intact.

Settings / Nearby play controls visibility. Discovery pauses while an Adventure,
profile entry, editing/modal or system/settings operation owns input. Existing
idle connections remain alive, but those interruptions do not accept new
activities. Bluetooth advertises the active Trainer name while TrainerOS owns
its visibility, restoring the previous adapter alias on cleanup if it still
owns that value. This does not rename saved identities or the system hostname.

## Bluetooth transport — 2026-09-30

The fixed root entry loads only its root-installed sibling module. BlueZ registers
the vendor service `91b83720-0abc-4a62-90ad-1bf7c3305d04` with a dynamically assigned
RFCOMM channel. Both client and server roles use the same existing Link v2
framing, heartbeat, saved identities, bilateral activity consent and protected
save transactions. No game adapter or settlement logic is replaced.

BR/EDR discovery uses a per-client UUID filter, RSSI threshold and scoped
discoverability. It validates service/name/address and bounds the list to 16;
rows expire after 180 seconds without a relevant update, accommodating sparse
inquiry updates. A stale row can fail its bounded ConnectProfile attempt and be
retried. Local-network rows identify their Bluetooth discovery UUID so the same
console is not listed twice; matching names alone never merge two Trainers.
Discovery UUIDs are not persistent game/Trainer identities. The accepted control
handshake supplies the real device identity before the existing native hello.

The helper creates one bounded worker for an RFCOMM descriptor. Its small control
handshake exchanges identity/name only, before the existing Accept/Decline popup.
It opens the game bridge only after Accept. The incoming side connects to
`127.0.0.1:47845`; the outgoing side exposes an ephemeral **loopback-only** port.
The relay preserves opaque native frames and bounds queued bytes to 64 KiB per
direction with backpressure. Connection/handshake/response/local-bridge deadlines,
EOF cleanup and generation checks prevent cancelled workers from releasing a
new session. Search resumes after disconnect; service/radio failure backs off
15 seconds. Existing bonds, power, saved Wi-Fi profiles and other Bluetooth
profiles are not removed or replaced.

The Bluetooth service uses application consent without requiring OS pairing or
a code comparison (`RequireAuthentication=false`, `RequireAuthorization=false`).
This is a casual nearby route, **not an authenticated/encrypted competitive
transport guarantee**. Existing bonds may protect an underlying connection, but
the implementation does not establish that guarantee for an unbonded peer.
Keep #93/#94 lineage/session trust gates open. The save transaction's exact-game,
identity and confirmation checks remain, but do not claim radio identity trust.

Default/missing policy selects Bluetooth. Only root policy
`/etc/traineros/nearby.json` with `"transport":"wifi-direct"` selects the retained
Direct implementation; its own `enabled` validation then applies. Odin's earlier
`{"enabled":false}` policy remains preserved and does not disable Bluetooth.
Do not re-enable Direct merely to recover an unavailable Bluetooth service.
`journalctl -t traineros-bluetooth` records allowlisted lifecycle stages/error
types, without Trainer names, payloads or exception messages.

Primary references: [BlueZ ProfileManager](https://raw.githubusercontent.com/bluez/bluez/master/doc/org.bluez.ProfileManager.rst),
[Profile descriptor lifecycle](https://raw.githubusercontent.com/bluez/bluez/master/doc/org.bluez.Profile.rst),
[scoped discovery and adapter alias](https://raw.githubusercontent.com/bluez/bluez/master/doc/org.bluez.Adapter.rst).

### Bluetooth device evidence and remaining limits

Both actual handhelds discovered each other over Bluetooth. An incoming
invitation appeared on Odin Home while only Flip was in Nearby play. Acceptance
established native Link through RFCOMM: Flip's native socket used
`127.0.0.1:58402 → 127.0.0.1:36419`, and Odin's incoming native socket used
`127.0.0.1:47845 ← 127.0.0.1:37498`; both helper journals recorded `ready` and
`relay-ready`. There was no inter-device LAN TCP Link connection in that check.
The consoles remained on their home Wi-Fi for SSH/capture; loopback sockets and
RFCOMM worker stages, rather than the UI label, establish the Bluetooth route.
This is not a physical Wi-Fi-off/airplane-mode acceptance test.

Declining a battle invitation retained the same connection. A reverse activity
invitation was accepted, opening the real saved-Party chooser, then cancelled
before reservation. Home/return kept the same TCP bridge. Earlier checks in this
increment also exposed an invitation timeout and a LAN invitation; neither is
counted as successful Bluetooth pairing. The initial 45-second discovery-row
expiry was extended for sparse BR/EDR inquiry, and the stale `Connecting` caption
was corrected for activity invitations after an accepted direct connection.

Native Windows and ARM builds passed. Four focused Windows C++ suites and six
ARM suites (including both Python helpers) passed; nine Bluetooth and thirteen
retained Direct helper checks also ran directly on Windows. ARM protocol tests
ran in a separate `--network none` container, never against the live shell's
port. Automated relay checks exercise fragmented control, consent gating,
decline/cancel, bounded duplex payloads, identity rejection and stale callbacks.

The helpers and production binary are installed on both devices with backups.
Each device retains its own library, Trainers, boot preference and save data;
no Link save reservation or settlement was made during this transport increment.
Repeated physical pairing is recorded in the final delivery note below.
Real battle completion/trade settlement over Bluetooth, additional radios,
prolonged reliability and authenticated trust remain separate gates. The earlier
Odin GPU hang, radio-driver lead and unproven Steam recovery are still unresolved;
do not call those fixed because a bounded Bluetooth session worked.

### Final Bluetooth delivery

Final production SHA-256 on both handhelds:
`1509f82a01ef3117bceccef636e522782246ca0fbbc1fea43d2d029bb9e3f963`.
Root Bluetooth module SHA-256:
`39c58377c322d4ae3bed15991555edbb368dd82ad10a774257d49129f1e7037c`;
fixed entry SHA-256:
`1dce71fa48f959f5e5e62cac0cdc35ed10f6875167d065617feba430fd10f936`.
Module/entry ownership and modes are root 0644/0755. Final live process checks
verified SQLite integrity, InputPlumber, Flip's three Trainers/830 registrations,
Odin's one/25, their individual boot preferences and no pending Link transaction.
The final build and six isolated ARM suites passed; four Windows C++ suites
passed again after the activity-caption correction.

Flip-to-Odin Bluetooth paired at 23:46:25. After final delivery, Odin-to-Flip
paired at 23:55:38 and retained the same loopback bridge through activity decline
and Home/return. The incoming battle popup on Flip Home used the corrected
activity caption. X disconnected the session; another invitation paired at
23:58:09 **without restarting either shell/helper**, using a new ephemeral bridge
`127.0.0.1:43954 → :51451` then `127.0.0.1:36200 → :51679` on Odin.
Both directions and one same-process reconnect are physical evidence, not a
long-duration/repeated-radio reliability claim. All times are local device time.

Both remained SSH-accessible. The sampled kernel journal since 23:32 had no
matching ath12k/hangcheck/preemption/GPU-fault lines, and no `traineros-nearby`
P2P stages occurred in that window. This is a bounded approximately 27-minute
Bluetooth session window with builds/restarts, not freeze causality proof.
Both were left on Home, disconnected from Link; the build container was stopped.

## Historical Wi-Fi Direct platform boundary

`NearbyService` owns the no-argument, root-installed `nearby-control.py` child.
Its bounded JSON verbs only configure Trainer identity/visibility, invite,
accept and disconnect. Wi-Fi Direct uses wpa_supplicant discovery and
NetworkManager's `wifi-p2p` connection type. The connection is volatile and
bound to its D-Bus client, never a replacement for saved Wi-Fi profiles.
EOF, termination or disconnect releases only that connection and restores the
previous discovery name/vendor data. A process lock prevents two shell helpers
from racing over the radio. The native local-network transport carries the
existing framed Link protocol (now version 2); old and new clients do not mix.

References: [NM Wi-Fi P2P device](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.Device.WifiP2P.html),
[Wi-Fi P2P settings](https://networkmanager.dev/docs/api/latest/settings-wifi-p2p.html),
[wpa_supplicant D-Bus](https://w1.fi/wpa_supplicant/devel/dbus.html).

## Connection recovery — 2026-09-30

Wi-Fi Direct response, accepted network creation and application pairing have
separate deadlines. Accepting near the end of the response window starts a fresh
network window; the old invitation timer cannot cancel address negotiation.

| Phase | Radio helper | Application guard |
| --- | --- | --- |
| Await invitation response | 50 seconds | 60 seconds |
| Create accepted network/address | 65 seconds | 75 seconds |
| Establish application session | Network already ready | 12-second TCP connect, 15-second pairing |

The outbound network window starts on the matching negotiation-success signal;
duplicate success signals cannot extend it repeatedly. The inbound window starts
on Accept. Discovery loss/expiry retains the current peer identity while that
attempt is active. Application handshake requires both existing accept messages
before exposing game data. LAN pairing and activity invitations retain their
separate 45-second window. Activity decline/expiry and page changes keep a paired
session; they never restart network creation.

Failed activation, matching negotiation failure, helper loss and connection
timeouts release the attempt and allow another invitation. A failed pending
connection reports its reason through the existing notice. The accepted
network state reads “Connecting to …”. Protected pending save transactions retain
their existing recovery state instead of being cancelled by a connection notice.

Disabling extended listening uses an empty D-Bus dictionary, as required by the
wpa_supplicant API above; zero-valued period/interval are invalid. Radio/service
initialization failures back off for 15 seconds. Cleanup still deactivates only
the helper's own volatile connection and preserves saved Wi-Fi profiles.

Focused regressions cover late acceptance, duplicate acceptance/negotiation,
activation retry, matching-peer cleanup, discovery loss, radio backoff, application
timeout/retry, both-side pairing, activity expiry/decline, page changes and
interrupted trade recovery. Network tests must run outside the live handheld's
network namespace: a host-network build container can otherwise connect to the
actual application's port 47845. These tests are not repeated-radio or GPU proof.

## Evidence and limits

Flip and Odin 2 both expose native P2P support. On the installed build, Flip
formed `10.42.0.1` and Odin received `10.42.0.173`; the application's established
TCP session used those P2P addresses on port 47845, while each retained its
192.168.50.x home-network address. An invitation appeared over Odin Home without
opening Link. Accept connected both; a battle invitation was declined and then
accepted without reconnecting, opening the saved-party chooser on both devices.
The activity was cancelled before any reservation/save mutation.

Repeat-connection reliability remains open. On the final installed build, one
outgoing attempt timed out before the receiver displayed an invitation. A later
attempt displayed the correct Home popover and formed a P2P group, but the
invitation deadline expired during address negotiation. A subsequent attempt
reported `supplicant-timeout`. Odin then still answered ping but stopped sending
an SSH banner; the cause is not yet established. The initiator was cancelled,
with no save operation started. Do not treat the earlier successful connection
as proof of stable repeated pairing. Next work must separate invitation-response
and accepted-network-establishment deadlines, expose actionable failures, and
verify reconnect/decline recovery on these two devices before expanding transport.

The search scene, Trainer row, connected portraits and both invitation popovers
were captured from the handhelds. Native Windows/ARM builds and focused protocol
and QML checks supplement device input. Other Wi-Fi chipsets, prolonged roaming,
Bluetooth transport, native battle-rule parity and competitive trust remain
separate work. This increment does not change the Emerald save adapter.

Odin's earlier frozen session retained a kernel thread in `dma_fence_default_wait`
after SIGTERM. Session restart and normal reboot did not recover it; the owner
power-cycled it. The new build runs after recovery. The original trigger has not
been established; do not describe the underlying GPU hang as fixed.
The saved kernel log records `hangcheck recover` against TrainerOS's
`QSGRenderThread` at 22:08:03 and `preemption timed out` ten seconds later.

## Delivery evidence — 2026-09-30

The production binary and required root-owned helper were updated on both Flip
and Odin, with database/binary/helper backups. Live production SHA-256 on both:
`0a92b9273863cb32daeb12b78977f26507cfc214f41ad6bb7f0dd57851373901`.
Final helper SHA-256:
`5ad63b8c9905481971455c68fd46d114e1d3ff6c5b581bb81f8758b75760dd18`.
Flip retained three Trainers/830 registrations; Odin retained one/25. Each device's
own library and boot preference were preserved. Odin entered its dedicated
TrainerOS session successfully from Steam under a bounded recovery guard; its
original absent boot override was restored. Controller input and actual compositor
captures worked on both, with SSH remaining available through this increment.
This does not establish the root cause or resolution of earlier GPU hangs.

Windows production-consumer build, focused Link/core/interaction/practice and
shell/Guide QML checks passed. ARM64 compilation and the five focused suites
passed in a separate network-isolated container. The final helper's ten checks
passed on both Windows and ARM Linux. An initial host-network protocol run was
invalid because it reached the live shell instead of its test listener; it is
not counted as protocol evidence. The workflow now documents the isolation rule.

During the device checks, Flip-to-Odin direct activation again reached
NetworkManager's 45-second `supplicant-timeout` before an incoming invitation.
The attempt released its volatile connection, displayed a failure notice and
allowed ordinary navigation/search without restarting the shell. The reverse
direct attempt also failed to establish a session. Ordinary LAN discovery and
an incoming invitation from Odin were observed after the first failure; this
does not prove direct pairing. No Link save reservation or settlement was started.
The final helper retains the original NetworkManager-controlled activation
sequence, without extra manual StopFind/listen changes at activation. Repeated
physical direct pairing, late Accept and activity-decline retention remain open
until demonstrated on that route. Automated coverage above is narrower evidence.

Next bounded work is the P2P discovery/negotiation failure itself on the current
Armada build, followed by repeated pair/decline/return proof. Do not expand battle,
save adapters or transport claims to conceal this remaining radio gate; keep the
following RA and image/update/system work in ROADMAP.

## Odin freeze isolation and temporary Direct policy — 2026-09-30

The owner reported another unresponsive Odin session. Before any new pairing
attempt in this increment, Odin still answered ping but stopped producing an
SSH banner. After the owner's reboot, the persisted previous-boot kernel journal
showed recurring `ath12k_wifi7_pci` vdev-1 lookup errors, firmware-stat failures
and timeouts during the background discovery window. It did not contain a new
GPU hangcheck/preemption event for this incident. Other user services continued
logging afterward. This is a radio-driver lead, not proof of a whole-kernel
freeze, an application deadlock or the cause of the separate earlier GPU hang.

A controlled session temporarily withheld only the Direct helper. The existing
production binary and normal graphics ran for about 19 minutes with controller
navigation and compositor captures. A six-minute observation recorded 37 thread/
load samples over 372 seconds, without sampled D-state render threads or matching
ath12k/hangcheck/preemption kernel lines. Two ordinary LAN connections worked;
declining a battle invitation retained the lobby, and leaving for Home and
returning retained its established TCP connection. No Link save reservation or
settlement was started. Short isolation evidence does not prove permanent recovery.

The helper now reads the root-managed `/etc/traineros/nearby.json` once on startup:

```json
{"enabled": false}
```

At this historical delivery, missing policy retained the Direct-enabled default. A malformed,
unreadable, oversized or non-boolean policy disables Direct. Disabled configure
keeps the helper command pipe alive but never initializes/scans/activates the
P2P radio. Ordinary LAN discovery and pairing remain separate and usable.
The temporary root-owned policy is installed **only on Odin**, not every Odin
model or Flip. It does not alter home-network profiles. Re-enable only during a
controlled recovery/driver investigation: set `enabled` to true or remove the
policy, then restart TrainerOS with no active game or protected operation.
The Bluetooth override above supersedes that default and re-enable advice:
Direct now additionally requires explicit `"transport":"wifi-direct"` selection.
Keeping this local policy through future image/OTA replacement remains an update
acceptance concern, not a delivered image-updater claim.

Allowlisted `traineros-nearby` system-journal events record initialization,
invitation, negotiation/activation stages, numeric NM device state/reason and
timeouts. They do not serialize signal dictionaries, credentials, WPS PINs,
Trainer names, peer paths or exception messages. Device-state observation does
not change network recovery behavior. Use `journalctl -t traineros-nearby` to
inspect these stages alongside the kernel journal.

Both devices received the helper atomically, with the unchanged production
binary SHA above. New live helper SHA-256:
`e132f8527e527bc24067799260df2dffc31b5785fb7f3731801bed099ff01294`.
The helper's 13 checks passed on Windows and ARM Linux. Live helper ownership,
InputPlumber, SQLite integrity, each device's own Trainer/library counts and
original boot preference were verified. Odin's journal reports disabled-by-policy;
Flip initializes normally. After the final installed-helper restart, a new
Odin-to-Flip session used `192.168.50.2:38986` → `192.168.50.206:47845` and
survived Home/return without reconnecting. No matching driver/render errors
appeared in the sampled post-delivery kernel journal. Both were left on Home.

The bounded control's recovery timer requested Steam, but the dedicated
TrainerOS process remained running: that request did **not** prove a working
Steam/session fallback. The temporary helper withholding was restored and
Odin's absent boot override preserved. Keep session-transition recovery as an
explicit open gate. Next work is freeze/radio causality and recovery, then
controlled Direct negotiation/reconnect proof; LAN success does not close the
no-router requirement or the separate GPU-hang investigation.

## Presentation names

Primary Pokémon section becomes **Companions**, Pokédex becomes **Field Guide**
(**Guide** in the secondary strip), and Pokémon Center becomes **Care Center**.
Actual catalogue/game/species names and persistent identifiers remain unchanged.
This is a product-label change, not a claim about intellectual-property clearance.
