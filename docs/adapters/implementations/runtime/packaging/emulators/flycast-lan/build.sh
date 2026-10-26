#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Separate native cabinet runtime; the installed Dojo GGPO binary stays intact.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${1:?Usage: build.sh NEW_WORK NEW_DEST}"
: "${2:?Choose a new destination}"
test ! -e "$1" && test ! -e "$2"
mkdir -p "$1" "$2/bin" "$2/licenses"
work=$(CDPATH= cd -- "$1" && pwd)
dest=$(CDPATH= cd -- "$2" && pwd)
git init "$work/source"
git -C "$work/source" remote add origin https://github.com/blueminder/flycast-dojo.git
git -C "$work/source" fetch --depth 1 origin d0e47e572b1e7b355e88bda8308c89d0c5156cbf
git -C "$work/source" checkout --detach FETCH_HEAD
git -C "$work/source" submodule update --init --recursive --depth 1
for patch in "$recipe/../flycast/traineros-launch.patch" "$recipe/../flycast/local-data-only.patch" "$recipe/native-inspect.patch" "$recipe/cabinet-session.patch" "$recipe/private-paths.patch"; do
  git -C "$work/source" apply --check "$patch"
  git -C "$work/source" apply "$patch"
done
git -C "$work/source/core/deps/glslang" apply "$recipe/../flycast/glslang-gcc.patch"
cmake -S "$work/source" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_HOST_SDL=ON -DUSE_VULKAN=ON -DUSE_OPENGL=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCPR_BUILD_TESTS=OFF -DCPR_BUILD_TESTS_SSL=OFF
cmake --build "$work/build" --parallel "${JOBS:-2}"
cp "$work/build/flycast-dojo" "$dest/bin/flycast"
cp "$recipe/../flycast/LICENSE.upstream" "$dest/licenses/Flycast.txt"
# Firmware and the shared content helper are explicitly supplied by packaging;
# no ROM, BIOS or personal progress is downloaded by this build.
