# Paired Armada maintenance — 2026-09-30

The owner requested upstream Armada updates on Flip 2 and Odin 2, plus the
current TrainerOS build on Odin. This is device maintenance, not delivery of
the planned TrainerOS image/OTA product in #70/#71.

## Verified delivery

| Device | Armada before | Booted after reboot | TrainerOS |
| --- | --- | --- | --- |
| Flip 2 | `20260924.1de6489` | `20260929.5915c28` | Existing production `04d004d` preserved |
| Odin 2 | `20260927.eba5232` | `20260929.5915c28` | Updated to production `04d004d` |

Both retained their existing `ghcr.io/armada-os/armada:testing` channel. The
reviewed upstream `/usr/libexec/armada/armada-update` checked availability,
staged signed updates through bootc and exited successfully. Reboots changed
the boot IDs; `bootc status` confirmed the new **booted**, not just staged,
deployment and retained a previous deployment for rollback. Kernel remained
`7.2.6`. Follow the [official update guidance](https://armadaos.dev/getting-started/updating/)
and inspect installed upstream helpers before future maintenance.

The live TrainerOS binary on each device was checked against SHA-256
`8893e2ac7ce74026d979bdd38a5ba5a6c497794f5b46597502a2100e09a2d4f2`.
Odin received the reviewed session/Adventure helpers without `--default`.
No accounts, saves or databases were copied between consoles. Private local
backups of each old binary, SQLite database and session configuration are in
`~/traineros-maintenance-20260930`; Odin's binary deployment also has its own
existing delivery-helper backup.

SQLite quick checks and before/after counts passed: Flip retained 3 Trainers
and 830 registered Adventures; Odin retained 1 Trainer and 25 registered
Adventures. These are registration counts, not a claim about ROM-file totals.
Configured boot preference hashes initially matched after both OS reboots.

## Runtime checks and Odin recovery

Flip returned automatically to its TrainerOS session. A bounded controller
Home → Worlds → Guide → Home sequence and actual Gamescope captures passed.
InputPlumber is active, production timing capture is off, and existing sleep,
suspend, hibernate and hybrid-sleep masks remain. Its current kernel journal
contains DisplayPort audio-routing errors; this pass does not claim to fix
unrelated device/audio issues.

Odin returned to its previous Steam boot choice. TrainerOS was then opened
through its existing session action for a live-binary check, which passed;
InputPlumber was active and diagnostic capture was off. Shortly afterwards
SSH stopped completing its protocol handshake although ping still responded.
No root cause is established, and the new OS is not claimed to cure the prior
Odin hang. A manual reboot was requested because remote restart was unavailable.

After the owner's manual reboot, SSH returned at the same address. Armada's
boot handling had already removed the temporary
`/etc/sddm.conf.d/zz-steamos-autologin.conf`; the full saved boot-configuration
hashes matched again, including the original `zz-holo-autologin.conf`. No manual
configuration replacement was needed. Bootc still reported `20260929.5915c28`,
the installed TrainerOS hash matched, and SQLite integrity/counts passed again.
Odin is currently in its original Steam session, with no running TrainerOS
process; its production binary remains installed. Flip's live production check
passed again. Do not describe this recovery as proof that the Odin hang is fixed.

The previous kernel/network/user-session journals were preserved privately on
Odin and copied to ignored operational files on the development machine. Before
loss of SSH, the kernel recorded repeated `ath12k_wifi7_pci` errors for virtual
device 1: `No VIF found`, `not found vif` and firmware-stat failure `-71`.
These provide a Wi-Fi investigation lead, not an established cause or a reason
to label this incident a proven GPU crash. Root-cause work and another Odin
session-entry attempt remain a separate increment.

The owner's follow-up reopened sleep assessment and isolated radio diagnostics.
See [the September 30 assessment](ARMADA_SLEEP_20260930.md) for exact upstream
changes, current per-device sleep policy and the unresolved radio evidence.
