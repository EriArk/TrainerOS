# PSP content identity reader

Linux-only read-only helper, separate from the installed PPSSPP executable.
It uses PPSSPP v1.20.4 `CISOFileBlockDevice` and `PBPReader`, pinned to
`fa50bb1976065c4f8b1b47af227d367fe9771555`, with libchdr headers pinned to
`8bba7745d758627258b315997a860039244cedaf`. No network/emulation code is called.
The shared bounded ISO9660/SFO parser is also used by TrainerOS for raw ISO.

Prerequisites: Linux, Git, CMake, Ninja, Python 3, C++20, Qt6 Core, zlib.
From the TrainerOS checkout (or the package's `source` directory):

```sh
sh tools/ppsspp-content/build.sh /absolute/new-build-directory
# Cross builds additionally pass -DCMAKE_TOOLCHAIN_FILE=/path/toolchain.cmake
python3 tools/ppsspp-content/test-reader.py /absolute/new-build-directory/build/psp-content-probe
# For ARM64 on x86: insert qemu-aarch64 -L /path/sysroot before the executable.
```

`prepare-source.py` verifies source revisions and consumed source changes. Its
only modification replaces a GUI read-error notification with a stored error.
ELF section collection discards unused readers; Windows PE linking is not part
of this recipe. Rebase by reviewing readers/bounds, updating pins in recipe and
TrainerOS, and rerunning native malformed/valid fixtures plus integration tests.
Do not update the emulator's multiplayer executable allowlist for this helper.

Package output includes architecture, binary SHA-256, upstream license and the
local source/recipe. Install the package directory as
`$XDG_DATA_HOME/traineros/psp-content` (default `~/.local/share/traineros/psp-content`)
alongside a shell containing the new probe. Verify the architecture and target
Qt/libc dependencies before delivery. Keep a previous directory for rollback;
removing this directory disables CSO/PBP admission, with raw ISO unaffected.
Installation is a release/device-delivery operation, not something this build
script performs. No ROMs, saves or PPSSPP configuration are changed.

Protocol: `psp-content-probe --psp-content /absolute/file.cso` (or `.pbp`) emits
one JSON object with `protocol: 1`, `disc`, `format`; invalid input returns nonzero
with no identity. The caller verifies the manifest/hash, cancels the child with
the scan, and bounds runtime to 15 seconds and output to 4096 bytes.

CSO v0/v1 is read through the native decompressor, with preflight bounds on
index spans, frame size and alignment. PBP reads outer PARAM.SFO through native
PBPReader; PSP categories UG/EG/MG and ELF/~PSP/NPUMD markers are required.
PS1 ME/PSISOIMG packages are rejected. A DISC_ID is format identity, not a
certificate of a working multiplayer mode or executable. NPUMD inner decryption
and boot are left to PPSSPP. Both players must still have identical whole-file
hashes: converting one player's ISO to CSO does not make it match the other's ISO.
CHD, wrappers, missing/unusual DISC_ID, and other categories remain unimplemented.
Synthetic cases prove header/decompression handling, not gameplay.
