#!/bin/sh
# Build the reviewed TCP relay repair with the same SDK generation as the
# installed Flatpak. The caller keeps the ordinary installation and saves.
set -eu
work=$(realpath -m "${1:?Usage: build.sh ABSOLUTE_WORK_DIRECTORY}")
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base=fa50bb1976065c4f8b1b47af227d367fe9771555
relay=572de778fabcbda819ab0f677368c3fbc3033e19
mkdir -p "$work"
if [ ! -d "$work/source" ]; then
    git clone --depth 1 --branch v1.20.4 https://github.com/hrydgard/ppsspp.git "$work/source"
fi
test "$(git -C "$work/source" rev-parse HEAD)" = "$base"
git -C "$work/source" submodule update --init --depth 1 --jobs 4 -- \
    assets/debugger ffmpeg ext/armips ext/glslang ext/SPIRV-Cross \
    ext/rapidjson ext/miniupnp ext/zstd ext/cpu_features ext/rcheevos \
    ext/naett ext/libchdr ext/lua ext/freetype ext/nanosvg ext/OpenXR-SDK
if [ ! -f "$work/source/ext/aemu_postoffice/.git" ]; then
    git -C "$work/source" submodule update --init --depth 1 -- ext/aemu_postoffice
fi
git -C "$work/source/ext/armips" submodule update --init --recursive --depth 1
git -C "$work/source/ext/aemu_postoffice" fetch --depth 1 origin "$relay"
git -C "$work/source/ext/aemu_postoffice" checkout --detach "$relay"
if ! git -C "$work/source" apply --reverse --check "$recipe/pdp-stat.patch" 2>/dev/null; then
    git -C "$work/source" apply --check "$recipe/pdp-stat.patch"
    git -C "$work/source" apply "$recipe/pdp-stat.patch"
fi
if ! git -C "$work/source" apply --reverse --check "$recipe/relay-retry.patch" 2>/dev/null; then
    git -C "$work/source" apply --check "$recipe/relay-retry.patch"
    git -C "$work/source" apply "$recipe/relay-retry.patch"
fi
if ! git -C "$work/source/ext/aemu_postoffice" apply --reverse --check "$recipe/socket-timeout.patch" 2>/dev/null; then
    git -C "$work/source/ext/aemu_postoffice" apply --check "$recipe/socket-timeout.patch"
    git -C "$work/source/ext/aemu_postoffice" apply "$recipe/socket-timeout.patch"
fi
cp "$recipe/relay-receive-test.c" "$work/relay-receive-test.c"
cp "$recipe/relay-timeout-test.c" "$recipe/relay-retry-test.cpp" "$work/"
flatpak run --command=sh --filesystem="$work" org.freedesktop.Sdk/aarch64/25.08 -eu -c '
    work=$1
    mkdir -p "$work/relay-test"
    cd "$work/relay-test"
    client="$work/source/ext/aemu_postoffice/client"
    gcc -O2 -g -I"$client/.." -c "$client/postoffice.c" \
        "$client/sock_impl_linux.c" "$client/postoffice_mem_stdc.c" \
        "$client/log_impl_stdc.c" "$work/relay-receive-test.c"
    g++ -O2 -g -c "$client/mutex_impl_cpp.cpp" "$client/delay_impl_cpp.cpp"
    g++ ./*.o -o relay-receive-test
    timeout 10 ./relay-receive-test
    mkdir -p "$work/timeout-test"
    cd "$work/timeout-test"
    gcc -O2 -g -DRELAY_IO_TIMEOUT_MS=300 -I"$client/.." -c "$client/postoffice.c" \
        "$client/sock_impl_linux.c" "$client/postoffice_mem_stdc.c" \
        "$client/log_impl_stdc.c" "$work/relay-timeout-test.c"
    g++ -O2 -g -c "$client/mutex_impl_cpp.cpp" "$client/delay_impl_cpp.cpp"
    g++ ./*.o -pthread -o relay-timeout-test
    timeout 10 ./relay-timeout-test
    g++ relay-timeout-test.o ../relay-test/postoffice.o ../relay-test/sock_impl_linux.o \
        ../relay-test/postoffice_mem_stdc.o ../relay-test/log_impl_stdc.o \
        ../relay-test/mutex_impl_cpp.o ../relay-test/delay_impl_cpp.o -pthread -o relay-production-timeout
    timeout 20 ./relay-production-timeout --production
    g++ -std=c++17 -O2 -I"$work/source" "$work/relay-retry-test.cpp" -o relay-retry-test
    ./relay-retry-test
    cmake -S "$work/source" -B "$work/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DUSING_QT_UI=OFF -DUSING_GLES2=ON \
        -DUSING_EGL=ON -DUSE_WAYLAND_WSI=ON -DUSE_SYSTEM_FFMPEG=OFF \
        -DUSE_SYSTEM_LIBZIP=ON -DUSE_SYSTEM_ZSTD=ON -DUSE_DISCORD=OFF \
        -DHEADLESS=OFF -DUNITTEST=OFF -DCMAKE_INSTALL_PREFIX=/app
    cmake --build "$work/build" --target PPSSPPSDL -j4
    sha256sum "$work/build/PPSSPPSDL"
' sh "$work"
