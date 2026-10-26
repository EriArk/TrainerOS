#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${1:?Usage: build.sh NEW_WORK NEW_DEST}"
: "${2:?Choose a new destination}"
test ! -e "$1" && test ! -e "$2"
mkdir -p "$1" "$2/bin" "$2/licenses"
work=$(CDPATH= cd -- "$1" && pwd)
dest=$(CDPATH= cd -- "$2" && pwd)
git init "$work/source"
git -C "$work/source" remote add origin https://github.com/flyinghead/flycast.git
git -C "$work/source" fetch --depth 1 origin 5aa091fde632fb332c8d8c34e280d62dc951954c
git -C "$work/source" checkout --detach FETCH_HEAD
git -C "$work/source" submodule update --init --recursive --depth 1
git -C "$work/source" apply --check "$recipe/traineros.patch"
git -C "$work/source" apply "$recipe/traineros.patch"
cmake -S "$work/source" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_HOST_SDL=ON -DUSE_VULKAN=ON -DUSE_OPENGL=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCPR_BUILD_TESTS=OFF -DCPR_BUILD_TESTS_SSL=OFF
cmake --build "$work/build" --parallel "${JOBS:-2}"
cp "$work/build/flycast" "$dest/bin/flycast"
cp "$work/source/LICENSE" "$dest/licenses/Flycast.txt"
# Caller supplies their own firmware. No ROM, BIOS or user saves are downloaded.
