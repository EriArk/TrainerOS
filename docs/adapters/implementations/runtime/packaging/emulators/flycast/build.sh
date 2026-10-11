#!/bin/sh
# Build on the target Linux architecture, with SDL2/OpenGL/Vulkan development
# packages, libzip, libcurl, Lua 5.3, CMake, a C++17 compiler and Python 3.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${1:?Usage: build.sh NEW_WORK NEW_DEST OWN_BOOT OWN_FLASH OPERATOR_UDP_URL [vulkan|opengl]}"
: "${5:?Missing operator relay endpoint}"
work=$1
dest=$2
test ! -e "$work" && test ! -e "$dest" || { echo 'Use new work/destination paths; retain old bundle for rollback' >&2; exit 1; }
mkdir -p "$work" "$dest"
work=$(CDPATH= cd -- "$work" && pwd)
dest=$(CDPATH= cd -- "$dest" && pwd)
git clone https://github.com/blueminder/flycast-dojo.git "$work/source"
git -C "$work/source" checkout --detach d0e47e572b1e7b355e88bda8308c89d0c5156cbf
git -C "$work/source" submodule update --init --recursive
git -C "$work/source" apply --check "$recipe/traineros-launch.patch"
git -C "$work/source" apply "$recipe/traineros-launch.patch"
cmake -S "$work/source" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DUSE_HOST_SDL=ON -DUSE_VULKAN=ON -DUSE_OPENGL=ON
cmake --build "$work/build" --parallel "${JOBS:-2}"
mkdir -p "$dest/bin" "$dest/firmware" "$dest/licenses"
cp "$work/build/flycast-dojo" "$dest/bin/flycast"
cp "$3" "$dest/firmware/dc_boot.bin"
cp "$4" "$dest/firmware/dc_flash.bin"
cp "$recipe/LICENSE.upstream" "$dest/licenses/Flycast.txt"
python3 "$recipe/bundle.py" "$dest" "$5" "${6:-vulkan}"
