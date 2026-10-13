# Paired Armada maintenance — 13 October 2026

Both handhelds were reachable after the owner powered them on. The installed
Armada helper was inspected and its read-only check found `20261012.fd0ed0f` on
their existing signed `ghcr.io/armada-os/armada:testing` channel. The normal
[Armada in-place update route](https://armadaos.dev/getting-started/updating/)
was used before TrainerOS device testing, without switching channels.

| Device | Before | Booted after reboot | Preserved data |
| --- | --- | --- | --- |
| Retroid Pocket Flip 2 | `20261010.94f52f8` | `20261012.fd0ed0f` | 3 Trainers, 849 registered Adventures |
| AYN Odin 2 | `20261010.94f52f8` | `20261012.fd0ed0f` | 1 Trainer, 75 registered Adventures |

Both had external power and no observed emulator process or pending Link journal.
Private backups of each database, binary and boot/session configuration preceded
the update. Both upstream systemd transactions exited 0; new boot IDs and the
booted bootc deployment confirmed activation. SQLite quick checks, registration
counts and saved boot-configuration hashes matched. Kernel remains `7.2.6`.
The existing TrainerOS binary hash remained unchanged through the OS update;
both shells started automatically. Odin's output volume remains 0%.

Odin had a failed external Batocera mount before maintenance; no failed system
unit was listed after reboot. This is not a separate network-storage acceptance.
Flip's initial battery reading was 14%; the fuller power-supply check confirmed
it was already charging before the update started.

This is upstream OS maintenance, not delivery of TrainerOS image/installer/OTA,
physical-control acceptance or new emulator gameplay evidence. Local operations
and backups remain private; no account/save data was copied between devices.
