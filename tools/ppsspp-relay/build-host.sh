#!/bin/sh
set -eu
# Reuse the reviewed aemu source already pinned by build.sh, never install a
# public daemon. Invoke inside the same Linux/ARM64 builder as stream_transport.
source=$(realpath "${1:?aemu_postoffice source}")
output=$(realpath -m "${2:?output binary}")
test "$(git -C "$source" rev-parse HEAD)" = 572de778fabcbda819ab0f677368c3fbc3033e19
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
# inet_pton returns zero for a nonmatching address family. Upstream tests only
# -1, accidentally turning an IPv4 loopback request into IPv6 wildcard binding.
git -C "$source" show HEAD:server_cpp/native_socket_linux.cpp > "$scratch/native_socket_linux.cpp"
patch --silent "$scratch/native_socket_linux.cpp" "$recipe/host-bind.patch"

g++ -std=c++20 -O2 -pthread -iquote "$source/server_cpp" \
  "$source/server_cpp/log.cpp" "$scratch/native_socket_linux.cpp" \
  "$source/server_cpp/session.cpp" "$source/server_cpp/server.cpp" \
  "$source/server_cpp/semaphore.cpp" "$recipe/host-main.cpp" -o "$output"
