#!/bin/sh
# Native Linux build candidate. This does not install or enable PS2 netplay.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${2:?Usage: build.sh NEW_WORK NEW_BUNDLE}"
work=$1
dest=$2
test ! -e "$work" && test ! -e "$dest" || { echo 'Use new paths; retain previous builds' >&2; exit 1; }
mkdir -p "$work/source" "$dest/licenses"
work=$(CDPATH= cd -- "$work" && pwd)
dest=$(CDPATH= cd -- "$dest" && pwd)
revision=5ad55b00aa2331b4c4cc7d990c6c32cab3a88359
git -C "$work/source" init -q
git -C "$work/source" remote add origin https://github.com/libretro/ps2.git
git -C "$work/source" fetch --depth 1 origin "$revision"
git -C "$work/source" checkout --detach FETCH_HEAD
git -C "$work/source" apply --check "$recipe/arm64-state-restore.patch"
git -C "$work/source" apply "$recipe/arm64-state-restore.patch"
cd "$work/source"
cmake -S . -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$work/build" --parallel "${JOBS:-2}"
cp "$work/build/bin/pcsx2_libretro.so" "$dest/pcsx2_libretro.so"
cp COPYING.GPLv3 "$dest/licenses/LRPS2-GPLv3.txt"
cp "$recipe/arm64-state-restore.patch" "$dest/arm64-state-restore.patch"
python3 - "$dest" "$revision" <<'PY'
import hashlib, json, sys
from pathlib import Path
root = Path(sys.argv[1])
files = {}
for name in ('pcsx2_libretro.so', 'arm64-state-restore.patch'):
    with (root / name).open('rb') as stream:
        files[name] = hashlib.file_digest(stream, 'sha256').hexdigest()
(root / 'build.json').write_text(json.dumps({
    'source': sys.argv[2], 'state': 'candidate', 'files': files
}, indent=2) + '\n', encoding='utf-8')
PY
