# Armada Iris encoder maintenance

This directory maintains the two encoder fixes recorded in
[the device evidence](../../../docs/ARMADA_IRIS_ENCODER.md). The current delivery
recipe builds a **complete, separately named Armada kernel and bootc image**.
The earlier module-only experiment is retained below as historical evidence;
it is not an installation procedure. None of these helpers switches the host,
writes its boot partition, unloads a driver or reboots it.

## Full system image: current recipe

The system build targets Armada `20261017.64207e6`, source
`64207e61a3c5b485815466533e14ac8a2b4ae76a`, Linux 7.2.6 and the exact native ARM64
compiler `gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)`. It applies all **214**
upstream kernel patches followed by the same two Iris patches below. The
upstream builder retains its configuration, device trees, full modpost/BTF and
module staging. Its concurrency is bounded and the release becomes
`7.2.6-traineros-iris1`; module trees cannot be mixed with the original kernel.

Use a disposable ARM64 build container with Python 3.12+, git, make, binutils,
patch, flex, bison, bc, elfutils/OpenSSL development headers, dwarves, zstd and
depmod. Keep at least 20 GiB free for the kernel build and additional space for
the complete base image. The builder stops below 6 GiB free. Fetch the exact
source object; a checkout is unnecessary:

```sh
git init armada-source
git -C armada-source fetch --depth=1 https://github.com/armada-os/armada.git 64207e61a3c5b485815466533e14ac8a2b4ae76a
curl -fLO https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.2.6.tar.xz
python3 packaging/kernel/iris/build-system-kernel.py \
  --linux-tarball linux-7.2.6.tar.xz --armada armada-source \
  --output full-build --jobs 4
python3 packaging/kernel/iris/prepare-system-image.py \
  --kernel-output full-build/out --armada armada-source --output image-context
podman pull ghcr.io/armada-os/armada@sha256:1f1f1996ec473517d9cd9831be20582e1938226152983241aa98f5588800924e
podman build --network=none --pull=never --layers=false \
  --tag localhost/traineros-armada:iris1 image-context
```

Output directories must be new. `build-receipt.json` records the source,
compiler, patch, configuration, Module.symvers and artifact hashes; only a
completed, matching receipt passes image preparation. The image is based on
`ghcr.io/armada-os/armada@sha256:1f1f1996ec473517d9cd9831be20582e1938226152983241aa98f5588800924e`.
Its installer refuses an OSTree host, validates the entire kernel archive before
replacing the image's kernel tree, and requires all 27 Armada DTBs. Armada's
pinned initramfs generator preserves and checks its splash and OSTree fallback;
`bootc container lint` must pass. `test-system-image.py` checks artifact admission
and rejects foreign paths, links and incomplete builds without touching a host.

Before delivery, inspect the complete build/image receipts, compare the resolved
configuration with the target and verify available storage. On the current
builder the resolved differences are the intentional local version, disabled
automatic local version and detected `CONFIG_OPENSSL_SUPPORTS_ML_DSA=y` build
tool capability. No device feature has been deliberately removed. This is not
a claim of a bit-identical rebuild.

Deploy through bootc only after the image passes validation. Preserve the
current deployment with an OSTree pin and verify Armada's existing ESP
`KERNEL.BAK` and boot-image finalization hooks. Keep an additional private backup
of the current boot files outside the ESP. Stage and inspect the candidate,
then reboot one handheld at a time; do not hot-replace Iris. After each boot,
verify the actual kernel release, module hash, TrainerOS startup and the
geometry/requested-IDR probe. Ordinary netplay does not use this encoder.

The installed bootc/OSTree tools support this explicit staging sequence (replace
the example layout path with the checksum-verified export). Prefer an OCI
directory on a volume with sufficient free space: importing an OCI archive can
also require temporary extraction space on the target's smaller internal disk.

```sh
podman save --format oci-dir --output traineros-iris.oci localhost/traineros-armada:iris1
# Record and verify index.json, oci-layout and every blob checksum after copying.
# On the target, after backups and checking there is no active game:
sudo ostree admin pin booted
sudo bootc switch --download-only --transport oci /path/to/traineros-iris.oci
sudo bootc status --json
# Inspect the staged kernel/modules/initramfs before unlocking it:
sudo bootc switch --from-downloaded
# Announce the restart; only then reboot and check the actual running build.
sudo systemctl reboot
```

`--download-only` retains a locked deployment which is not applied by a reboot.
Do not use `--apply` as a substitute for inspecting the staged image. For a
running candidate, `sudo bootc rollback`, inspection of `bootc status --json`
and a separately announced reboot select the prior deployment. Rollback reuses
that deployment's `/etc`; it does not merge later configuration changes back.

Armada's existing shutdown/finalization hooks build the device boot image from
the staged deployment. A previous deployment and backup image are recovery
mechanisms, not proof of a successful recovery test. If boot fails, use Armada's
retained fallback/recovery path; never blindly overwrite boot partitions. A
working candidate can return to the pinned original with `bootc rollback` and
the same inspected reboot path. Switching to a local image changes bootc's
tracked source: record that source and explicitly switch back to the accepted
Armada channel when ending the experiment. Ordinary Armada updates can replace
the patched kernel until the patches are integrated upstream.

Build, installed boot, encoder checks and gameplay acceptance remain separate.
See the device evidence for the actual delivery state; generating this recipe
does not mean either handheld is running the new kernel.

## Historical module experiment: exact source and changes

- Linux [v7.2.6](https://github.com/gregkh/linux/tree/500df175a7f9e6bc1a9c328590ca5150f84f9ff0),
  kernel.org `linux-7.2.6.tar.xz`, SHA256
  `039aef84f2b0994aeda3f4fcfc3d02ec9d7a9bbb9020ea264c43f446c860f606`.
- Armada [fd0ed0f1a285fbbccc5fa63969d4e176cb0ae4e6](https://github.com/armada-os/armada/tree/fd0ed0f1a285fbbccc5fa63969d4e176cb0ae4e6/packages/kernel),
  all 196 patches in its kernel series, matching image `20261012.fd0ed0f`.
- `0001`: unmodified upstream Linux
  [6f62dcefd2494aa9ac01538372353bf07755491e](https://github.com/torvalds/linux/commit/6f62dcefd2494aa9ac01538372353bf07755491e),
  Wangao Wang's requested-keyframe support for both Iris generations.
- `0002`: TrainerOS's bounded gen1 correction: pass visible `enc_raw_width/height`
  to HFI encoder input frame size instead of aligned NV12 storage dimensions.
  The decoder branch is unchanged. Apply after `0001`.

The patched kernel sources retain **GPL-2.0-only** and their author notices;
the original upstream patch retains its authors/review/sign-off metadata. See
[the upstream GPL text](LICENSE.upstream). The standalone build helper uses the
TrainerOS GPL-3.0-or-later license; it is not linked into the kernel module.
No firmware or binary is bundled here. Image/source-distribution acceptance #115
remains separate.

## Reproduce the module

Use an ARM64 build container with the device kernel's exact compiler:
`gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)`. Required tools include Python 3.12+,
git, make, binutils, patch, flex, bison, bc, elfutils/OpenSSL development headers.
The tested container also has dwarves. Extract `/proc/config.gz` from the target
device into `device.config`; do not substitute a generic defconfig.

```sh
curl -fLO https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.2.6.tar.xz
git init armada-source
git -C armada-source fetch --depth=1 https://github.com/armada-os/armada.git fd0ed0f1a285fbbccc5fa63969d4e176cb0ae4e6
python3 packaging/kernel/iris/build-module.py \
  --linux-tarball linux-7.2.6.tar.xz --armada armada-source \
  --config device.config --output iris-build
```

The output directory must be new. The helper validates the archive, reads Armada
from the pinned git object, verifies every patch hunk with zero fuzz, checks
configuration drift, prepares the kernel and builds only `qcom-iris.ko` with two
jobs. It never installs anything. The result is under
`iris-build/linux-7.2.6/drivers/media/platform/qcom/iris/`.

This module-only build lacks the original `Module.symvers`/`vmlinux`, so modpost
reports unresolved exports and BTF is skipped. `CONFIG_MODVERSIONS` must be off.
These warnings are **not permission to force a module into an arbitrary kernel**.
Before temporary loading, compare the target's complete config/compiler, module
vermagic, architecture and undefined imports against its original `qcom-iris.ko`.
Check every import against the running kernel's `__ksymtab_*` exports. For the
recorded devices the import sets were identical and all imports were exported.
Keep the installed dependencies loaded; a production build must use the full
Armada kernel build and its actual `Module.symvers`.

## Historical temporary verification and rollback

The following describes the earlier experiment, **not a supported update
procedure**. Owned diagnostic streams were stopped and both video-device users
and `/sys/module/qcom_iris/refcnt` were checked. The original module remained on
disk, both hashes were recorded, and the verified candidate was temporarily
loaded with `insmod` after a normal unload. No force unload or version-check
bypass was used. The external build set the kernel's out-of-tree taint flag;
that flag was recorded separately from kernel failures. The later Odin failure
below invalidated hot replacement as a delivery path.

Run `python3 tools/probe-iris-encoder.py`: it checks all visible band boundaries
and requires a real H.264 IDR/SPS within 15 frames after a requested keyframe.
A GStreamer accepted-event result or periodic I-picture alone cannot pass.
Then run the paired stream with `drop_frame=180` (all data and parity removed)
and optionally `drop_every=30`. Verify the client's IDR request, new received IDR,
continued decoded video/input, and a captured game image without the bottom band.

On Flip the diagnostic session returned to the original using normal
`rmmod qcom_iris` followed by `modprobe qcom_iris`, with references at zero.
**On Odin, unload after codec use hung in `__synchronize_irq` from
`iris_vpu_power_off`/`iris_core_deinit`.** The ordinary reboot request was accepted
but SSH did not return during the check; physical recovery was requested.
Later on 14 October the owner's reboot restored both devices; the original
module hash and active TrainerOS sessions were verified. This does not resolve
the unload defect or establish a safe hot-replacement path.
Do not repeat hot replacement on Odin or describe it as an accepted update path.
The system module remains unchanged; the next boot uses the original. A production
change requires a recoverable full image and verified shutdown/reboot behavior.
The out-of-tree taint bit stays until reboot. Never force-unload the stuck driver.

## Update policy

For a new Armada kernel, inspect upstream first: drop `0001` once included and
check whether `0002` has been superseded. Refresh source pins/config/compiler,
rebuild using that kernel's full build metadata, then repeat only the affected
geometry, requested-IDR and paired loss/display checks on gen1 and gen2. Do not
carry a 7.2.6 binary forward or claim the whole Remote Play feature is delivered.
