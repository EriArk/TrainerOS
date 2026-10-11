# Paired Armada maintenance — 11 October 2026

The owner powered on both handhelds and requested an Armada system update before
resuming emulator tests. This is maintenance of the development devices, not a
TrainerOS image/installer/OTA release or new multiplayer acceptance.

## Update record

| Device | Before | Booted after update | Verification |
| --- | --- | --- | --- |
| AYN Odin 2 | `20261006.5ea7609` | `20261010.94f52f8` | New boot ID, signed bootc deployment, automatic TrainerOS start |
| Retroid Pocket Flip 2 | `20260929.5915c28` | `20261010.94f52f8` | New boot ID, signed bootc deployment, automatic TrainerOS start |

Both retain the existing `ghcr.io/armada-os/armada:testing` channel. The installed
`/usr/libexec/armada/armada-update` helper was inspected before use; its signed
bootc update path was used. No channel switch, independent Flatpak update or
manual bootloader flash was requested or performed. Follow the
[official update guidance](https://armadaos.dev/getting-started/updating/) and
inspect the installed helper again on future maintenance.

The selected system image digest is
`sha256:1efebeb1215caa915b1231760dc9b0c5ccb6f9c182c59b48a3d48679315bdadb`.
Record completion from the **booted** deployment, not only downloaded/staged
metadata. Preserve the preceding deployment for rollback.

Before maintenance both devices were identified by their device-tree model,
were at 100% battery, ran TrainerOS and had no observed game-emulator process or
failed system unit. The temporary idle build container on Flip was stopped before
its system update. No emulator test was run ahead of the requested update.

The installed TrainerOS executable before maintenance matched on both devices:
SHA-256 `65d838038bc5ce6be4d5847230ec6a6469a5e1b69a5da056a1582f284f3127ff`.
This is the existing installed client, not the newer Windows development build.

## Post-boot evidence

Odin: bootc reports the selected image as booted, no staged deployment, and the
previous `20261006.5ea7609` deployment available for rollback. Kernel is `7.2.6`.
TrainerOS's user service is active/running, its executable hash is unchanged,
InputPlumber and Armada Control are active, and no failed system units are listed.
The default audio output remains at 0%. The ordinary Armada boot-image service
reports its kernel image is current.

Flip: bootc likewise reports the selected image as booted with no staged
deployment, retaining `20260929.5915c28` for rollback. Kernel is `7.2.6`.
TrainerOS's user service is active/running with the unchanged executable hash;
InputPlumber and Armada Control are active, and no failed system units are listed.
Its ordinary boot-image service also reports the kernel image is current.

Both upstream update transactions completed with exit status 0. An SSH command
waiting for Odin's background updater timed out; the independently running
system service completed successfully and was inspected before reboot. It was
not launched twice. No physical controls, audio, suspend, extended
stability, DS game or cross-network multiplayer result is inferred from these
system/service checks. Accounts, game files, saves and databases were not copied
between handhelds by this maintenance.

The system-update prerequisite is complete. The incomplete
[DS work](emulators/melonds.md) resumes on this baseline; retain all earlier
multiplayer, Hotseat and release gates.
