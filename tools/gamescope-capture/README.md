# Gamescope capture lifetime repair

Bounded companion build for Armada's `terra-gamescope-3.16.31^1-1.100601.fc44.armada`.
This is a compositor fix needed by game capture, not a new streaming stack.
`sources.json` pins OGC `afd6eac6`, Armada `fd0ed0f1`, its 26 patches with SHA256,
and the reused punktfunk buffer-association fix. Retain both license files.
The original `destroy_buffer` assertion remains enabled; corruption is not hidden.

On Linux with Git, Python 3.12+, Meson, Ninja and the dependencies from the pinned
Terra spec (including Fedora `catch-devel`, not the older `catch2-devel`):

```sh
python3 tools/gamescope-capture/prepare.py /work/gamescope-capture-source
meson setup /work/gamescope-capture-build /work/gamescope-capture-source \
  --prefix=/usr --buildtype=release -Db_ndebug=false \
  -Dpipewire=enabled -Ddrm_backend=enabled \
  -Denable_openvr_support=false -Denable_gamescope_wsi_layer=false \
  -Dforce_fallback_for=vkroots,wlroots,libliftoff
meson compile -C /work/gamescope-capture-build -j2
meson test -C /work/gamescope-capture-build --print-errorlogs
```

This builds only a companion executable; existing Armada WSI layer, scripts and
device configuration are retained. OpenVR is disabled for these handheld probes.
The development build uses the pinned upstream header submodules; Terra's
system-header substitutions are not needed to compile it. This is not a
byte-identical rebuild of the RPM. Check dynamic dependencies before deployment.

`install.py BINARY REVIEWED_SHA256` installs beside `/usr/bin/gamescope` as root
only when the system executable matches the reviewed baseline. Install the
repository's `packaging/session/compositor.sh` and the license files alongside it.
Finish live games normally before restarting the TrainerOS graphical session.
No kernel reload, RPM replacement or automatic restart is performed by this tool.

The launcher checks both executable hashes before selecting the companion. An
OS update or modified binary uses the ordinary system compositor. Rebuild and
revalidate capture against that new baseline before restoring the override.
To roll back, rename `/var/opt/traineros/system/gamescope` to a backup directory,
then restart the graphical session after closing games. Keep prior builds and
user data; never delete saves to reset a capture test.

Build/tests alone do not prove the crash fixed: disconnect the capture consumer
while a real game remains alive, verify the same game/compositor processes, then
exercise a new capture on that same compositor. See
[device evidence and remaining limits](../../docs/REMOTE_PLAY_TRANSPORT.md).
