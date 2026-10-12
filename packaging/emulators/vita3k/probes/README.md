# Native LAN probe provenance

This is a private compatibility probe, not a bundled game. No firmware, game
assets, VPK or shareware PAK is distributed by this directory.

Source: [vitaQuake 2.1](https://github.com/Rinnegatamante/vitaQuake/tree/045f0e3d000f8b8eba8a763c4343445811ac39d1),
revision `045f0e3d000f8b8eba8a763c4343445811ac39d1` (GPL-2.0-or-later).
The patch retains the upstream licence of each modified file. It updates old
VitaSDK headers, RTC calls, audio enum conversions and static linking for the
reviewed toolchain. The native UDP socket binds `INADDR_ANY` so its existing
LAN-discovery code can receive broadcasts; binding the sole unicast address
previously allowed direct connection but failed native server search on Linux.
No game or emulator network protocol is replaced.

Toolchain: official VitaSDK ARM64 snapshot `20260926.790.1`, archive
`vitasdk-aarch64-linux-gnu-2026-09-26_08-31-11.tar.bz2`, SHA-256
`741e6184fcc8c73d700bf84de1dea5659aabafecd6e6dac0d443361ee73e67fd`.
The nine official package archives and their downloaded hashes are recorded in
[packages.json](packages.json). Unpack them into the SDK's `arm-vita-eabi`
prefix, retaining each package licence. Build in an isolated Linux ARM64
environment with the SDK `bin` on PATH:

```sh
git apply /path/to/vitaquake-2.1-vitasdk.patch
cd source
make -j2 vitaQuake.velf
vita-make-fself -s vitaQuake.velf ../build/eboot.bin
vita-mksfoex -s TITLE_ID=QUAK00001 vitaQuake ../build/sce_sys/param.sfo
```

Package `build/eboot.bin` and `build/sce_sys/` at the VPK root with a ZIP writer;
the old Makefile's VPK recipe uses Windows paths and is not the Linux packaging
step. The reviewed rebuild retained the previous probe's generated SFO/assets
and replaced only `eboot.bin`. That artifact has these hashes:

- EBOOT: `4ffe7203efdf0071c67681e00910065a0dc5db8a2087e822fef8c9b317a97b4e`.
- VPK: `8a79741ab9d318ec514cb1fec8cfa5bb30aa202f67793dfcb3fec1d1bcd0935b`.

The original Quake 1.06 shareware archive was obtained from the public
[idsoftware mirror](https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip),
SHA-256 `ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239`.
Its `id1/pak0.pak` is personal test content; no DOS installer was executed.

The source embeds build date/time. The recipe is maintained source provenance,
not proof that another build produces identical bytes. Review and retest a new
EBOOT before changing the strict installed profile. Existing native saves,
accounts, controls and game assets must not be replaced by this rebuild.
