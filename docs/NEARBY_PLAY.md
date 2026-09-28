# Nearby play — 2026-09-28

The consoles advertise the active Trainer's name outside the Link screen.
Nearby play uses Wi-Fi Direct when both devices expose its native route, with
ordinary local-network discovery as a fallback. No router is required for the
direct route. This replaces the earlier same-router/page-open/code-comparison UX.

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
activities. Visibility here describes TrainerOS, not system Bluetooth names.

## Platform boundary

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

## Evidence and limits

Flip and Odin 2 both expose native P2P support. On the installed build, Flip
formed `10.42.0.1` and Odin received `10.42.0.173`; the application's established
TCP session used those P2P addresses on port 47845, while each retained its
192.168.50.x home-network address. An invitation appeared over Odin Home without
opening Link. Accept connected both; a battle invitation was declined and then
accepted without reconnecting, opening the saved-party chooser on both devices.
The activity was cancelled before any reservation/save mutation.

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

## Presentation names

Primary Pokémon section becomes **Companions**, Pokédex becomes **Field Guide**
(**Guide** in the secondary strip), and Pokémon Center becomes **Care Center**.
Actual catalogue/game/species names and persistent identifiers remain unchanged.
This is a product-label change, not a claim about intellectual-property clearance.
