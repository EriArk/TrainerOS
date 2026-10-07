#!/bin/sh
# Run inside the ARM64 Qt/build container. OUTPUT must be a new directory.
set -eu
patch_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:?Usage: build.sh NEW_OUTPUT_DIRECTORY}
test ! -e "$out"
mkdir -p "$out"
out=$(cd "$out" && pwd)
test "$(uname -m)" = aarch64
for entry in \
 'gpsp https://github.com/libretro/gpsp.git 5819380c2ffb0900219d700a382ee68c464ebb99' \
 'doublecherry https://github.com/TimOelrichs/doublecherryGB-libretro.git 03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb'; do
 set -- $entry
 name=$1; url=$2; revision=$3
 git clone --no-checkout "$url" "$out/$name"
 git -C "$out/$name" checkout --detach "$revision"
 test "$(git -C "$out/$name" rev-parse HEAD)" = "$revision"
 if [ "$name" = gpsp ]; then
  make -C "$out/$name" -j2 platform=unix CPU_ARCH=arm64 HAVE_DYNAREC=1
 else
  # Upstream mixes LF and CRLF; exported Windows archives can add another CR.
  # Normalize only the five patched text files before applying the reviewed diff.
  for file in libretro/dmy_renderer.cpp libretro/dmy_renderer.h \
      libretro/inline/inline_functions.h libretro/libretro.cpp \
      libretro/libretro_core_options.h; do
   tr -d '\r' < "$out/$name/$file" > "$out/$name/$file.lf"
   mv "$out/$name/$file.lf" "$out/$name/$file"
  done
  git -C "$out/$name" apply --check "$patch_dir/doublecherry-rtc.patch"
  git -C "$out/$name" apply "$patch_dir/doublecherry-rtc.patch"
  cp "$patch_dir/TrainerMbc3Rtc.h" "$out/$name/libretro/"
  make -C "$out/$name" -j2 platform=unix
 fi
done
# Keep sources/licenses beside artifacts. Install through emulator-core-bundle.py,
# not directly over an active emulator. Compiler changes may change the digest.
mkdir "$out/cores"
cp "$out/gpsp/gpsp_libretro.so" "$out/cores/"
cp "$out/doublecherry/DoubleCherryGB_libretro.so" "$out/cores/"
sha256sum "$out/cores/"*.so
