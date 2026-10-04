#!/bin/sh
# Run inside the ARM64 Qt/build container. OUTPUT must be a new directory.
set -eu
out=${1:?Usage: build.sh NEW_OUTPUT_DIRECTORY}
test ! -e "$out"
mkdir -p "$out"
out=$(cd "$out" && pwd)
test "$(uname -m)" = aarch64
for entry in \
 'gpsp https://github.com/libretro/gpsp.git 5819380c2ffb0900219d700a382ee68c464ebb99' \
 'doublecherry https://github.com/TimOelrichs/doublecherryGB-libretro.git 03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb'; do
 set -- $entry
 name=$1; url=$2; revision=$3
 git clone --no-checkout "$url" "$out/$name"
 git -C "$out/$name" checkout --detach "$revision"
 test "$(git -C "$out/$name" rev-parse HEAD)" = "$revision"
 if [ "$name" = gpsp ]; then
  make -C "$out/$name" -j2 platform=unix CPU_ARCH=arm64 HAVE_DYNAREC=1
 else
  make -C "$out/$name" -j2 platform=unix
 fi
done
# Keep sources/licenses beside artifacts. Install through emulator-core-bundle.py,
# not directly over an active emulator. Compiler changes may change the digest.
mkdir "$out/cores"
cp "$out/gpsp/gpsp_libretro.so" "$out/cores/"
cp "$out/doublecherry/DoubleCherryGB_libretro.so" "$out/cores/"
sha256sum "$out/cores/"*.so
