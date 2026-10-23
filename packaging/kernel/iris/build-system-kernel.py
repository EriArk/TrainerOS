#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a complete, separately named Armada kernel; never install or reboot.

Uses the pinned upstream builder, including its device trees, configuration,
full modpost/BTF, modules staging and archive. See README.md for image delivery.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import tarfile
import shutil
import time

ARMADA = "64207e61a3c5b485815466533e14ac8a2b4ae76a"
LINUX_SHA256 = "039aef84f2b0994aeda3f4fcfc3d02ec9d7a9bbb9020ea264c43f446c860f606"
KERNEL = "7.2.6-traineros-iris1"
PATCHES = ("0001-upstream-request-keyframe.patch", "0002-gen1-visible-input-size.patch")


def digest(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--linux-tarball", type=Path, required=True)
    p.add_argument("--armada", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--jobs", type=int, choices=range(1, 5), default=2)
    p.add_argument("--prepare-only", action="store_true")
    args = p.parse_args()
    if platform.machine() != "aarch64":
        p.error("Use the verified native ARM64 toolchain")
    if digest(args.linux_tarball) != LINUX_SHA256:
        p.error("Expected the pinned Linux 7.2.6 archive")
    compiler = subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0]
    if compiler != "gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)":
        p.error("Compiler differs from the verified Armada toolchain")
    for command in ("make", "patch", "flex", "bison", "bc", "pahole", "zstd", "depmod"):
        if not shutil.which(command):
            p.error(f"Missing build dependency: {command}")
    # Archive the immutable revision, never a developer's working tree.
    archive = subprocess.check_output([
        "git", "-C", str(args.armada), "archive", ARMADA, "packages/kernel"])
    args.output.mkdir(parents=True, exist_ok=False)
    root = args.output.resolve()
    if shutil.disk_usage(root).free < 20 * 1024**3:
        raise RuntimeError("Need at least 20 GiB free for the full debug/BTF build")
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as tar:
        tar.extractall(root, filter="data")
    package = root / "packages/kernel"
    series = package / "patches/series"
    entries = [s.split("#")[0].strip() for s in series.read_text().splitlines()]
    if len([s for s in entries if s]) != 214:
        raise RuntimeError("Unexpected Armada patch count")
    patch_hashes = {}
    for name in PATCHES:
        original = Path(__file__).resolve().parent / name
        shutil.copyfile(original, package / "patches" / name)
        patch_hashes[name] = digest(original)
    with series.open("a") as f:
        f.write("\n# TrainerOS Iris fixes\n" + "\n".join(PATCHES) + "\n")
    # Keep upstream configuration/DTB/build/staging logic. Only cap concurrency
    # and give the complete build its own release so it cannot mix module trees.
    builder = package / "scripts/build-kernel.sh"
    source = builder.read_text()
    if source.count("JOBS=$(nproc)") != 1:
        raise RuntimeError("Upstream builder concurrency contract changed")
    builder.write_text(source.replace("JOBS=$(nproc)", f"JOBS={args.jobs}"))
    config = package / "config/armada-kernel.config.overrides"
    with config.open("a") as f:
        f.write('\nCONFIG_LOCALVERSION="-traineros-iris1"\n# CONFIG_LOCALVERSION_AUTO is not set\n')
    work = root / "build"
    work.mkdir()
    shutil.copyfile(args.linux_tarball, work / "linux-7.2.6.tar.xz")
    manifest = {"armada_commit": ARMADA, "linux_sha256": LINUX_SHA256,
                "kernel_release": KERNEL, "compiler": compiler,
                "patches": patch_hashes, "jobs": args.jobs,
                "status": "prepared", "installed": False}
    receipt = root / "build-receipt.json"
    receipt.write_text(json.dumps(manifest, indent=2) + "\n")
    if args.prepare_only:
        print("PREPARED_ONLY", root, flush=True)
        return
    env = dict(os.environ, WORK_DIR=str(work), OUT_DIR=str(root / "out"),
               KBUILD_BUILD_USER="armada", KBUILD_BUILD_HOST="traineros-builder")
    for name in ("FAST", "KERNEL_VERSION", "KERNEL_SRC_URL", "KCONFIG_CONFIG",
                 "KBUILD_OUTPUT", "KBUILD_MODPOST_WARN", "ARCH", "CROSS_COMPILE",
                 "KCFLAGS", "HOSTCFLAGS", "KAFLAGS", "MAKEFLAGS"):
        env.pop(name, None)
    def interrupted(signum, _frame):
        raise InterruptedError(f"Build interrupted by signal {signum}")
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    # No module-only warning override, no hot loading, no changes to the OS.
    process = subprocess.Popen(["bash", str(builder)], env=env, start_new_session=True)
    try:
        while process.poll() is None:
            if shutil.disk_usage(root).free < 6 * 1024**3:
                raise RuntimeError("Stopping build to retain 6 GiB device free space")
            time.sleep(5)
        if process.returncode:
            raise RuntimeError(f"Full kernel builder failed: {process.returncode}")
    except BaseException as error:
        manifest.update(status="failed", error=str(error))
        receipt.write_text(json.dumps(manifest, indent=2) + "\n")
        raise
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    source = work / "linux-7.2.6"
    if (source / "include/config/kernel.release").read_text().strip() != KERNEL:
        raise RuntimeError("Unexpected output kernel release")
    for name in ("Module.symvers", "vmlinux"):
        if not (source / name).is_file() or not (source / name).stat().st_size:
            raise RuntimeError(f"Full build did not produce {name}")
    artifact = root / "out" / f"armada-kernel-{KERNEL}.tar.zst"
    manifest.update(status="built", artifact=artifact.name,
                    artifact_sha256=digest(artifact),
                    config_sha256=digest(source / ".config"),
                    module_symvers_sha256=digest(source / "Module.symvers"))
    receipt.write_text(json.dumps(manifest, indent=2) + "\n")
    shutil.copyfile(receipt, root / "out/build-receipt.json")
    print("FULL_KERNEL_BUILT_NOT_INSTALLED", artifact, flush=True)


if __name__ == "__main__":
    main()
