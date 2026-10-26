#!/bin/sh
set -eu
# Run from a TrainerOS source checkout. Optional remaining arguments go to CMake.
work=$1
shift
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$work"
work=$(CDPATH= cd -- "$work" && pwd)
if [ ! -d "$work/source" ]; then
    git clone --depth 1 --branch v1.20.4 https://github.com/hrydgard/ppsspp.git "$work/source"
fi
git -C "$work/source" submodule update --init --depth 1 ext/libchdr
cmake -S "$recipe" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPPSSPP_SOURCE="$work/source" "$@"
cmake --build "$work/build" -j 3
python3 "$recipe/package.py" "$work/build/psp-content-probe" "$work/source" "$work/package"
