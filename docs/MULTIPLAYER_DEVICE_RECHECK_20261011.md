# Earlier multiplayer routes on updated handhelds — 11 October 2026

**12 October follow-up:** the N64 and Dreamcast build-only statements in this
11 October record are historical. Later installed native gameplay and ordinary
invitation/Home/exit evidence is in [RMG](emulators/RMG.md) and
[Flycast](emulators/flycast.md). Their remaining gates are retained there.

Owner order: verify previously prepared routes before returning to DS. Both
Flip 2 and Odin 2 boot the [updated Armada system](ARMADA_UPDATE_20261011.md).
This record separates native builds, installed client checks and actual games.
DS work is deferred during this pass; no multiplayer issue is closed.

## Client and focused Linux checks

Built the current client and six affected test executables in the existing
Fedora 44 ARM64 build container (GCC 16.2.1, Qt 6.11.2). `standalone`, `retroarch`,
`game_party`, `runtime_multiplayer` and `rmg_netplay` passed initially.
`flycast_netplay` initially failed because its fake executable lacked Unix
execute permission. The fixture now first proves rejection without permission,
then grants owner execution; the corrected suite passes. Production validation
was already rejecting non-executable bundles and remains intact.

The RMG real-server cases were then explicitly enabled with the pinned ARM64
Go binary and a temporary loopback certificate. Both Nearby and TLS four-client
room/roster/start checks pass with no skipped cases in that invocation. This is
lobby protocol evidence, not four running N64 games. The first TLS attempt had
no generated certificate because the container lacked the OpenSSL CLI; after
installing that build dependency and creating the certificate, the case passed.

Installed client SHA-256 on both handhelds:
`ccfe46697e26df086f39099f537afc5f08f8b00577a116b0f69960006f1c6633`.
Previous binaries and consistent SQLite backups were retained privately before
replacement. Initial save inventories covered 9 files on Flip and 28 on Odin.
Existing user profiles and emulator choices were retained. Ordinary emulator
configuration was not replaced; runtime-written changes are recorded below.

## Native emulator builds

RMG at the [maintained pin](emulators/RMG.md) builds and installs into a private
staging directory, including its ARM64 core and Vulkan/OpenGL plugins. The
handheld host still lacks `libminizip.so.1`, `libSDL3_net.so.0` and
`libspeexdsp.so.1`; an isolated runtime dependency bundle or supported image
packages are required before promotion. The staged binary also needs the recipe's
installed RPATH (the diagnostic configure omitted it). No operator online server
has been deployed, and no production N64 bundle is advertised by this check.
Staged RMG executable SHA-256:
`e38bb5e11d85b229e209825095cc5d19ae07dc0c51f587bde375f5857749e8ad`.

Flycast Dojo at the [maintained pin](emulators/flycast.md) completes the full
ARM64 build with the native launch/relay patch. Host `ldd` reports no missing
libraries for the resulting executable. SHA-256:
`3292d8c16687e7e063bdc826c0dd5328f11cb0c422086bed8ff5e192db181dff`.
This is a built artifact, not paired Dreamcast gameplay or a deployed relay.

Build corrections retained in the recipe:

- The pinned glslang header now includes `<cstdint>` explicitly for GCC 16.
- CMake 4 receives `CMAKE_POLICY_VERSION_MINIMUM=3.5`; unrelated CPR dependency
  tests are disabled when building the emulator.
- The Linux build no longer downloads mutable shared NVRAM/save packs or
  optional training data. That upstream download command failed before fetching
  anything because `wget` was absent. Local firmware and private fresh VMUs remain
  the maintained integration policy.

Build-only Fedora dependencies added in the existing container include
`SDL3_net-devel`, `minizip-devel`, `speexdsp-devel`, `libsamplerate-devel`,
`qt6-qtsvg-devel`, `libpng-devel`, `libzstd-devel`, `freetype-devel`, `hidapi-devel`,
`sdl2-compat-devel`, `asio-devel`, `libzip-devel`, `lua-devel`, `golang`, `openssl`
and `which`. They were not layered into Armada's system image. Existing Qt,
Vulkan, USB, compiler and other build packages remain prerequisites.

## PS1 paired check

CTR - Crash Team Racing uses PCSX ReARMed, two digital ports and private temporary
cards. Matching core SHA-256 on both devices:
`25bfeb6336d7ef573903775fe53733af974c90edd268cf4fb9afa497e188609d`;
RetroArch runtime:
`8d57b47c88d416c05145187c8c906b7ab0013edb7e45af69d9b6bb841aea4a24`.
Exact disc/sidecar identity:
`69e3a0d0d2a4cc714a7b4ee654f05ff48a1ab2c4785054c2612bae449766197e`.

Odin initially had no PS1 library registration for the existing disc. The normal
maintenance importer added that one existing owner file, with a backed-up and
locked store; it did not copy a ROM or save. Nearby invitation, notification,
explicit acceptance, two-person readiness, host save/leave confirmation and
automatic launch on both handhelds were observed. The native TCP connection was
established. Both devices reached a two-player Crash Cove race (Crash/Tiny).
Separate host and guest input moved their own karts; both screens showed the
same race. This is a short paired gameplay check, not a complete race or a
compatibility matrix. Host Home/Exit returned to the shell. The disconnected
guest remained in RetroArch and was closed through its own Home/Exit; automatic
closure of both copies is not claimed. All 9/28 pre-existing save hashes remained
unchanged after this check. Both devices share one router.

## GameCube paired check

The existing exact Melee US revision 1 profile completed a new Nearby invitation,
explicit acceptance, 2/4 readiness and host save/leave barrier. Initial launch
screens persisted during preparation; subsequent observation found both native
Dolphins running, so the early observation was not a proven startup failure.
Both copies created only their isolated temporary card, selected Mario/Ness with
independent controllers and reached an Onett versus match. Movement/attack input
was exercised on both handhelds. Guest Home/Exit/Leave closed its owned process;
peer departure also closed the host and returned it to the library.

A read-only call of the current generic admission implementation on Odin's real
Auto Modellista US ISO produced `runtime.dolphin.gc.v1`, two ports and settings
`dolphin-gc-pads-isolated-v1`. Content identity:
`d310f8a7ba12bec57f8820127c482134a3d07b9a5cc40b742b3f31d8546a086c`.
The existing bridge hash is
`30c959ca95242b9011526b89a0d9f626d4cff9f1bf77482bb9cf603870d6660b`.
This proves admission of a non-Melee owner disc on the device; it does not prove
paired Auto Modellista gameplay or universal GameCube compatibility.

## PSP paired check

The installed client completed Nearby Lumines invitation/acceptance, the host's
save/leave confirmation, isolated PPSSPP launch on both handhelds and native
2P VS discovery. Both private configurations retained the exact Lumines
`ForceLagSync2=True` policy, WLAN enabled, relay mode 2 and host-only Ad Hoc server.
The ordinary configurations still had WLAN disabled and their prior clock policy.

The first native game challenge returned to the lobby. A second promptly accepted
challenge reached skin selection and a live two-player arena; movement, rotation
and drop inputs were exercised independently on both devices and their screens
showed the paired arena. This is a short round check, not first-attempt reliability
or prolonged stability. Home/Exit/Leave closed both owned PPSSPP processes and
removed their temporary `.traineros-netplay-*` configuration directories.

An earlier observation of a ready party without a guest process was a UI-test
mistake: the Start action was above the scrolled view, and a coordinate click hit
the information row. Physical-input navigation revealed Start and the real launch
then succeeded. No emulator launch failure is inferred from that observation.
The changing roster retains a disabled former action and may require scrolling;
compact party presentation remains a usability limitation of this check.

A read-only generic parser call also recognized the owner's real PATAPON UMD
(`UCUS98711`) with `runtime.ppsspp.adhoc.v1` and native clock settings. PATAPON is
not a multiplayer gameplay example: disc-format admission does not establish
an Ad Hoc mode, and ordinary explicit solo metadata still gates invitations.
No second multiplayer-capable PSP title was available on both devices for this
bounded check; wider generic gameplay and private native room admission remain open.

## Final integrity and scope

After all games closed, 28/28 pre-existing Odin file hashes remained unchanged.
On Flip, 8/9 remained unchanged; the sole difference was ordinary Dolphin's
68-byte `GC/SRAM.raw`, timestamped 22:06:52 during the ordinary Melee exit before
the paired process started. No pre-existing memory-card or PSP/RetroArch game-save
hash changed, no baseline file disappeared and no new file appeared in the
inventoried roots. This inventory includes the SRAM runtime file; do not summarize
it as every file unchanged. No baseline was restored over current user data.
Both SQLite `quick_check` results were `ok`. No tested emulator process remained;
both TrainerOS session services were active, Flip muted at 0% and Odin at 0%.

The fixtures, native builds and short paired checks above do not close broad
compatibility, four physical clients, pass-controller Hotseat, distinct networks,
NAT/relay/fresh-device provisioning, physical comfort/audio or release/licensing.
RMG needs a host-compatible dependency bundle and operator service; Dojo needs
operator relay deployment/review and actual installed native gameplay. Generic
GameCube/PSP need wider-title gameplay. DS was not changed during this pass.
Private diagnostics/captures and backups stay on the test machines/work directory;
no ROM, firmware, save, account data or executable is committed.

## Repository validation

Final adapter export/check passed with 32 semantic source files and 87 runtime
source files. The updated shell recipe passes `sh -n`; both added build patches
reverse-check against the successfully built pinned ARM64 source. Line endings
were normalized to LF after a raw Windows transfer exposed a shell syntax error.
The six affected Linux suites passed across the initial run and the corrected
Flycast-only rerun; already-passed suites were not repeated solely for reporting.
