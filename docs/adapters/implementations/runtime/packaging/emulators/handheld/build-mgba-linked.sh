#!/bin/sh
# Reproducible ARM64 linked-GBA core; leave the ordinary solo mGBA untouched.
set -eu
patch_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:?Usage: build-mgba-linked.sh NEW_OUTPUT_DIRECTORY}
test ! -e "$out"
test "$(uname -m)" = aarch64
mkdir -p "$out"
out=$(cd "$out" && pwd)
revision=b66511a35873bda9c961872813e58b35543bcfc5
git clone --no-checkout https://github.com/Spuds0588/mgba-splitscreen.git "$out/source"
git -C "$out/source" checkout --detach "$revision"
test "$(git -C "$out/source" rev-parse HEAD)" = "$revision"
git -C "$out/source" apply --check "$patch_dir/mgba-linked-pair.patch"
git -C "$out/source" apply "$patch_dir/mgba-linked-pair.patch"
cmake -S "$out/source" -B "$out/build" -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG -DCOLOR_16_BIT -DCOLOR_5_6_5" \
 -DLIBMGBA_ONLY=ON -DBUILD_LIBRETRO_SPLITSCREEN=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF
cmake --build "$out/build" --target mgba_splitscreen_libretro -j2
mkdir "$out/cores"
cp "$out/build/mgba_splitscreen_libretro.so" "$out/cores/"
sha256sum "$out/cores/mgba_splitscreen_libretro.so"
# Keep source + LICENSE together. Deploy through emulator-core-bundle.py.
