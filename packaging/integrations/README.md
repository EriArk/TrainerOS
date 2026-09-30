# Adventure exit transport

`adventure-overlay.py` is an opt-in, ordinary-user helper for the verified Flip 2
InputPlumber/Gamescope X11 environment. It requires Python 3 with `dbus`,
libX11/libXRes 1.2, ffmpeg with libdav1d and read-only access to the built-in pad.
`overlay_support.py` must be beside it. Other controller profiles fail closed.
The current exit question is enabled only for owned RetroArch windows. Keep
standalone emulator exits until their own confirmation/close paths are validated.

The shell's private `integrations/overlay.json` enables it:

```json
{
  "version": 1,
  "enabled": true,
  "helper": "/var/opt/traineros/integrations/adventure-overlay.py"
}
```

Only a dedicated TrainerOS session with a real library uses this configuration.
The helper starts with an owned game process and establishes its independent
watchdog before requesting InputPlumber mode 1. Physical Guide changes mode to 2.
Its JSON protocol uses stdin/stdout, with GUI heartbeats and generation-tagged
physical input. No listening port is opened. Capture precedes the Qt question;
close uses XRes-verified process ancestry and WM_DELETE_WINDOW, never a kill.

Install all three Python files root-owned in a root-owned, non-writable parent.
**Before granting raw access**, install the reviewed `flip-input-source.conf`
environment for the device's graphical sessions and their child emulators.
Merge any existing SDL ignore list instead of replacing it. On this Flip the
physical source is USB `2020:3001`; the InputPlumber Xbox target is `045e:0b12`.
Without the filter, SDL may choose the grabbed physical source and receive no
button events. The helper's ioctl reads remain available with the SDL filter.
See [SDL's ignore-list contract](https://github.com/libsdl-org/SDL/blob/SDL2/include/SDL_hints.h).

For the current session, add the matching `Environment=` entry to its systemd
unit drop-in, reload the user manager and restart the shell with no game active.
Persist the value in the graphical-login environment as well; future maintenance
and emulator sessions must inherit it. Check the effective SDL device inventory
and a launched emulator, not only the environment file's presence.

`grant-input-read.py ACCOUNT` is the only root hook. It waits up to ten seconds
for InputPlumber's hidden **Retroid Pocket Gamepad** source, checks its kernel
identity and grants only that account read access through its open descriptor.
It grants no general input-group membership and no write access. Run it after
InputPlumber initialization, for example in a reviewed service drop-in:

```ini
[Service]
ExecStartPost=/usr/bin/python3 /var/opt/traineros/integrations/grant-input-read.py ACCOUNT
```

Replace ACCOUNT with the verified local session account. Back up existing
files/drop-ins and reload systemd. Do not restart InputPlumber during gameplay.
An absent controller must not prevent the underlying service from starting.
The current hook targets the built-in pad at service startup; controller/device
replacement needs revalidation. Verify access again after a real reboot.

Rollback: explicitly disable `overlay.json`, restore the backed-up binary/helpers
and drop-in, reload systemd, and remove this account's named ACL from the exact
verified source when no helper is active before retiring the SDL source filter.
Keep a working maintenance exit until
physical Home/A/B and restoration pass for each adapter. Custom legacy exit bindings
are not removed by these scripts. Saves and state files are not migrated.

### Odin 2 / PPSSPP addition (2026-09-28)

The helper also recognizes the verified `AYN Odin2 Gamepad` USB source
2020:3001 with axes 0–5 and triggers 2/5. Flip keeps its own axes 0–5/20/21
and trigger profile. Odin's separate paddle source is not mistaken for its pad.
The narrow read-only ACL hook accepts either built-in name; it does not expose
all input devices. The session client selects Odin's `xbox-elite` InputPlumber
target after Steam's Deck target, without modifying the global device profile.

Owned `PPSSPPSDL` windows now share the guarded exit path with RetroArch.
Executable validation supplements the existing XRes/process-start ownership
checks. PPSSPP uses `--pause-menu-exit` so TrainerOS's confirmation is followed
by a graceful close, rather than a second emulator question. Other standalone
emulators still need their own device/exit proof.

`install-radio-control.py --user ACCOUNT` adds the fixed-verb NetworkManager/
BlueZ helper and sudo policy. Run from a trusted root-owned installation source;
it preserves previous helper/policy copies and does not toggle radios itself.
See [network delivery and limits](../../docs/DEVICE_NETWORK_PLAN.md).

The same installer includes the no-argument process-owned `nearby-control.py`
entry and its root-owned `nearby-bluetooth.py` module (0644). Bluetooth/BlueZ
RFCOMM is the default route; install python3-dbus, PyGObject, BlueZ and kernel
RFCOMM support on the supported image. The existing fixed sudo entry is unchanged;
the shell cannot choose arbitrary Python modules or commands. Discovery and
the profile belong to the helper's D-Bus lifetime; existing bonds/radio power and
other profiles stay intact. Explicit root policy `{"transport":"wifi-direct"}`
selects the retained NetworkManager/wpa_supplicant Direct implementation instead.
Its separate `enabled` policy still applies; there is no automatic P2P fallback.
The installer does not create/change a device's local policy. See
[Nearby play behavior and proof](../../docs/NEARBY_PLAY.md).

### Session installation and DS/Dolphin Home (2026-09-28)

The session installer now includes prerequisite checks, canonical helper
installation and the read-only ACL hook. Use
`python3 packaging/session/install.py --user ACCOUNT --emulator-support-only`
for an existing supported device. It keeps the session/default and emulator
preferences intact. Stock legacy bridges redirect to the installed helper after
backup; custom bridges remain unchanged. Start+Select no longer bypasses guarded
Home in the stock dedicated-session bridge.

Without `overlay.json`, dedicated TrainerOS sessions now use the installed
canonical helper. Explicitly disabled/invalid configurations remain disabled.
DS and GameCube launch/cancel/close were verified on Flip; Wii and other routes
retain separate proof gates. See [behavior, sources and evidence](../../docs/STANDALONE_HOME_EXIT.md).
