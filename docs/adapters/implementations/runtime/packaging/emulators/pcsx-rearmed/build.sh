#!/bin/sh
# Separate CBIN runtime; never replaces the ordinary PS1 core.
set -eu
archive=${1:?git archive of the pinned revision required}
work=${2:?new work directory required}
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$(sha256sum "$archive" | cut -d ' ' -f1)" = b8ff3af97fe864d76be625d8d9836b1030996def010bc95b5aa2d8bd50e24a96
test ! -e "$work"
mkdir -p "$work/source" "$work/package"
tar -xf "$archive" -C "$work/source"
git -C "$work/source" apply --check "$recipe/cbin-reader.patch"
git -C "$work/source" apply "$recipe/cbin-reader.patch"
python3 "$recipe/check-cbin.py" --source "$work/source" --binary "$work/check-cbin"
make -C "$work/source" -f Makefile.libretro platform=arm64 GIT_VERSION=\"\ c881679-cbin1\" -j2
cp "$work/source/pcsx_rearmed_libretro.so" "$work/package/core.so"
cp "$work/source/COPYING" "$work/package/LICENSE.upstream"
cp "$recipe/cbin-reader.patch" "$work/package/"
gzip -n -c "$archive" > "$work/package/source.tar.gz"
cp "$recipe/build.sh" "$recipe/check-cbin.py" "$work/package/"
python3 - "$work/package" <<'PY'
from pathlib import Path
import hashlib,json,sys
root=Path(sys.argv[1])
files={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ('core.so','cbin-reader.patch')}
sources={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ('source.tar.gz','build.sh','check-cbin.py')}
(root/'manifest.json').write_text(json.dumps({'protocol':'pcsx-cbin-v1','source':'c8816799b50388e61cfe237fe2cdbb7d8175f20a','files':files,'sources':sources},indent=2)+'\n')
PY
