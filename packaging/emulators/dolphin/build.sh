#!/bin/sh
# Run inside the maintained ARM64/Qt build environment. Never modifies a device install.
set -eu
source_dir=${1:?pinned Dolphin checkout required}
build_dir=${2:?build directory required}
recipe_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$(git -C "$source_dir" rev-parse HEAD)" = c77bbaa0f372c3f72281602a8b087206706542cb
# An already applied patch is accepted; any other source drift requires review.
if git -C "$source_dir" apply --check "$recipe_dir/traineros-netplay.patch" 2>/dev/null; then
    git -C "$source_dir" apply "$recipe_dir/traineros-netplay.patch"
else
    git -C "$source_dir" apply --reverse --check "$recipe_dir/traineros-netplay.patch"
fi
cp "$recipe_dir/TrainerNetplay.inc" "$source_dir/Source/Core/DolphinQt/TrainerNetplay.inc"
cmake -S "$source_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TESTS=OFF -DENABLE_LLVM=OFF -DENCODE_FRAMEDUMPS=OFF \
    -DUSE_DISCORD_PRESENCE=OFF -DENABLE_AUTOUPDATE=OFF -DLINUX_LOCAL_DEV=ON
cmake --build "$build_dir" --target dolphin-emu -j3
