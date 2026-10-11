#!/bin/sh
# Native core/local frontend preparation. Does not replace the ordinary emulator.
# Linux x86_64/aarch64: Git, CMake >=3.16, Ninja, C/C++17, Python3, Qt6 Widgets, SDL2.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${1:?Usage: build.sh NEW_WORK_DIRECTORY}"
work=$1
test ! -e "$work" || { echo 'Use a new work directory; existing builds are retained' >&2; exit 1; }
mkdir -p "$work"
work=$(CDPATH= cd -- "$work" && pwd)
git init -q "$work/source"
git -C "$work/source" fetch --depth=1 https://github.com/melonDS-emu/melonDS.git 906e9ebb27da8c6a715cd7abab4abfe8a8d29427
git -C "$work/source" checkout --detach FETCH_HEAD
git -C "$work/source" apply --check "$recipe/traineros-detmp.patch"
git -C "$work/source" apply "$recipe/traineros-detmp.patch"
cmake -S "$recipe" -B "$work/build" -G Ninja -DMELONDS_SOURCE="$work/source" \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_JIT=ON
cmake --build "$work/build" --target core traineros_detmp_checks traineros_ds_input_checks traineros_ds_spi_checks traineros_ds_gpu_checks traineros_ds_cartridge_state_checks traineros_ds_owned_state_probe traineros_ds_prepared_checks traineros_ds_pair_probe traineros_ds_local --parallel "${JOBS:-2}"
ctest --test-dir "$work/build" --output-on-failure
python3 "$recipe/source_manifest.py" "$work/source" "$work/build/source-manifest.json"
