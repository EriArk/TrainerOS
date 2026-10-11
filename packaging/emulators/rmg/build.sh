#!/bin/sh
# Pinned upstream native netplay; requires the dependencies in docs/emulators/RMG.md.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${1:?Usage: build.sh NEW_WORK_DIRECTORY DESTINATION ONLINE_WSS_URL_OR_--nearby-only [vulkan|opengl]}"
: "${2:?Missing destination}"
: "${3:?Supply an operator WebSocket endpoint or --nearby-only}"
work=$1
dest=$2
test ! -e "$work" || { echo 'Use a new work directory' >&2; exit 1; }
test ! -e "$dest" || { echo 'Use a new destination; retain the previous bundle for rollback' >&2; exit 1; }
mkdir -p "$work" "$dest"
work=$(CDPATH= cd -- "$work" && pwd)
dest=$(CDPATH= cd -- "$dest" && pwd)
git clone https://github.com/Rosalie241/RMG.git "$work/RMG"
git -C "$work/RMG" checkout --detach 4777ed3f1fb7f1e4a0e70438a27dc4a5e9fb1927
git -C "$work/RMG" apply --check "$recipe/traineros-launch.patch"
git -C "$work/RMG" apply "$recipe/traineros-launch.patch"
cp "$recipe/TrainerNetplay.inc" "$work/RMG/Source/RMG/UserInterface/TrainerNetplay.inc"
cmake -S "$work/RMG" -B "$work/build" -DCMAKE_BUILD_TYPE=Release \
    -DPORTABLE_INSTALL=OFF -DNETPLAY=ON -DVRU=OFF -DUSE_ANGRYLION=OFF \
    -DUPDATER=OFF -DAPPIMAGE_UPDATER=OFF -DCMAKE_INSTALL_PREFIX="$dest" \
    -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib'
cmake --build "$work/build" --parallel "${JOBS:-2}"
cmake --install "$work/build"
git clone https://github.com/Rosalie241/rmg-netplay-server.git "$work/server"
git -C "$work/server" checkout --detach bf0f28746971fcb6341fac55c0d2d19f646a6e09
git -C "$work/server" apply --check "$recipe/server-tls.patch"
git -C "$work/server" apply "$recipe/server-tls.patch"
(cd "$work/server" && go build -trimpath -o "$dest/bin/rmg-netplay-server" .)
mkdir -p "$dest/licenses"
cp "$work/server/LICENSE" "$dest/licenses/rmg-netplay-server.txt"
cp "$recipe/LICENSE.upstream" "$dest/licenses/RMG.txt"
if [ "${ARMADA_BUNDLE_DEPS:-0}" = 1 ]; then
    python3 "$recipe/armada-dependencies.py" "$dest"
fi
python3 "$recipe/bundle.py" "$dest" "$3" "${4:-vulkan}"
