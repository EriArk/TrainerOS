#!/bin/sh
# Isolated PS2 candidate. Never replace the ordinary RetroArch installation.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${2:?Usage: build-retroarch.sh NEW_WORK NEW_BUNDLE}"
work=$1
dest=$2
test ! -e "$work" && test ! -e "$dest" || { echo 'Use new paths; retain previous builds' >&2; exit 1; }
mkdir -p "$work" "$dest/licenses"
work=$(CDPATH= cd -- "$work" && pwd)
dest=$(CDPATH= cd -- "$dest" && pwd)
revision=69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576
git -C "$work" init -q
git -C "$work" remote add origin https://github.com/libretro/RetroArch.git
git -C "$work" fetch --depth 1 origin "$revision"
git -C "$work" checkout --detach FETCH_HEAD
git -C "$work" apply --check "$recipe/retroarch-state-history.patch"
git -C "$work" apply "$recipe/retroarch-state-history.patch"
cd "$work"
# When the build host is newer than the deployment runtime, supply that
# runtime's link libraries (e.g. libm.so and libasound.so), never host replacements.
# Keep these outside the source/bundle; the installed runtime owns them.
if [ -n "${TARGET_LIBS:-}" ]; then
    test -d "$TARGET_LIBS"
    LDFLAGS="-L$TARGET_LIBS ${LDFLAGS:-}"
    export LDFLAGS
fi
./configure --enable-networking --enable-command --enable-vulkan --enable-x11 \
    --enable-udev --enable-alsa --enable-sdl2 --disable-qt --disable-ffmpeg \
    --disable-mpv --disable-pipewire --disable-pulse --disable-jack \
    --disable-systemd --disable-dbus --disable-wayland --disable-libdecor \
    --disable-cheevos --disable-discord --disable-online_updater --disable-cg \
    --disable-kms --disable-vulkan_display
make -j"${JOBS:-2}"
cp retroarch "$dest/retroarch"
cp COPYING "$dest/licenses/RetroArch-GPLv3.txt"
cp "$recipe/retroarch-state-history.patch" "$dest/retroarch-state-history.patch"
python3 - "$dest" "$revision" <<'PY'
import hashlib, json, sys
from pathlib import Path
root = Path(sys.argv[1])
files = {}
for name in ('retroarch', 'retroarch-state-history.patch'):
    with (root / name).open('rb') as stream:
        files[name] = hashlib.file_digest(stream, 'sha256').hexdigest()
(root / 'build.json').write_text(json.dumps({
    'source': sys.argv[2], 'state': 'candidate', 'stallFrames': 8,
    'files': files
}, indent=2) + '\n', encoding='utf-8')
PY
