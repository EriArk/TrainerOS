#!/bin/sh
# Fresh source from the official unified release; no device installation changes.
set -eu
archive=${1:?official azahar-unified-source-2126.2.tar.xz required}
work_dir=${2:?new build workspace required}
recipe_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$(sha256sum "$archive" | cut -d ' ' -f1)" = 85c8b56fac703df04720125044b8ff887997911858a6cde545dfb1107ba4cd26
test ! -e "$work_dir"
mkdir -p "$work_dir/source"
tar -xf "$archive" -C "$work_dir/source"
source_dir="$work_dir/source/azahar-unified-source-2126.2"
git -C "$source_dir" apply --check "$recipe_dir/traineros-room.patch"
git -C "$source_dir" apply "$recipe_dir/traineros-room.patch"
git -C "$source_dir" apply --check "$recipe_dir/libusb-init-failure.patch"
git -C "$source_dir" apply "$recipe_dir/libusb-init-failure.patch"
cp "$recipe_dir/TrainerRoom.inc" "$source_dir/src/citra_qt/TrainerRoom.inc"
cmake -S "$source_dir" -B "$work_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DUSE_SYSTEM_QT=ON -DUSE_SYSTEM_SDL2=ON -DENABLE_LTO=OFF -DENABLE_TESTS=OFF \
    -DENABLE_QT_TRANSLATION=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_SCRIPTING=OFF \
    -DENABLE_GDBSTUB=OFF -DENABLE_OPENGL=OFF -DENABLE_VULKAN=ON \
    -DENABLE_QT_UPDATE_CHECKER=OFF
cmake --build "$work_dir/build" -j2
