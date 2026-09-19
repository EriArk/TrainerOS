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

Rollback: disable/remove `overlay.json`, restore the backed-up binary/helpers
and drop-in, reload systemd, and remove this account's named ACL from the exact
verified source when no helper is active before retiring the SDL source filter.
Keep a working maintenance exit until
physical Home/A/B and restoration pass for each adapter. Legacy exit bindings
are not removed by these scripts. Saves and state files are not migrated.
