#!/bin/sh
# Run in the Fedora ARM builder. Host Qt/GIO remain system dependencies.
set -eu
destination=${1:?Usage: bundle-files-runtime.sh OUTPUT_DIRECTORY}
mkdir -p "$destination"
cp -L /usr/lib64/libfm-qt6.so.17 "$destination/libfm-qt6.so.17"
cp -L /usr/lib64/libmenu-cache.so.3 "$destination/libmenu-cache.so.3"
patchelf --set-rpath '$ORIGIN' "$destination/libfm-qt6.so.17"
rpm -q libfm-qt menu-cache > "$destination/packages.txt"
rpm -q --qf '%{NAME} %{SOURCERPM} %{LICENSE}\n' libfm-qt menu-cache > "$destination/sources.txt"
for package in libfm-qt menu-cache; do
    mkdir -p "$destination/licenses/$package"
    rpm -ql "$package" | while IFS= read -r path; do
        case "$path" in /usr/share/licenses/*) [ ! -f "$path" ] || cp "$path" "$destination/licenses/$package/";; esac
    done
done
(cd "$destination" && sha256sum libfm-qt6.so.17 libmenu-cache.so.3 > SHA256SUMS)
