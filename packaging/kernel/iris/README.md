# Armada Iris encoder maintenance

This is a **diagnostic kernel-module build**, separate from TrainerOS's installed
application and image. It repairs the two encoder defects recorded in
[the device evidence](../../../docs/ARMADA_IRIS_ENCODER.md). No autoload, system
module replacement, firmware change or full kernel installation is performed.

## Exact source and changes

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

## Temporary verification and rollback

Stop only the owned diagnostic streams. Confirm no process holds `/dev/video0`
or `/dev/video1` and `/sys/module/qcom_iris/refcnt` is zero. Keep the original
module on disk unchanged and record both hashes. Unload `qcom_iris` normally and
load the verified candidate with `insmod`; **never force unload or bypass version
checks**. If loading fails, immediately `modprobe qcom_iris` from the unchanged
system module tree. A successful external build sets the kernel's out-of-tree
taint flag; it is recorded, not treated as a kernel failure.

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
