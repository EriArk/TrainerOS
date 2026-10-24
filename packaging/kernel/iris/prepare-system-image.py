#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a bounded bootc build context from a completed full kernel build.

Does not pull/build an image, switch the host, update boot partitions or reboot.
The base is pinned to the Armada deployment used for Iris validation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ARMADA = "64207e61a3c5b485815466533e14ac8a2b4ae76a"
BASE = "ghcr.io/armada-os/armada@sha256:1f1f1996ec473517d9cd9831be20582e1938226152983241aa98f5588800924e"
KERNEL = "7.2.6-traineros-iris1"
ARTIFACT = f"armada-kernel-{KERNEL}.tar.zst"
PATCHES = ("0001-upstream-request-keyframe.patch", "0002-gen1-visible-input-size.patch")


def digest(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def validate(kernel_output):
    receipt = json.loads((kernel_output / "build-receipt.json").read_text())
    if (receipt.get("status"), receipt.get("armada_commit"),
        receipt.get("kernel_release"), receipt.get("artifact")) != (
            "built", ARMADA, KERNEL, ARTIFACT):
        raise ValueError("Not a completed pinned full-kernel build")
    if not receipt.get("module_symvers_sha256") or not receipt.get("config_sha256"):
        raise ValueError("Missing full build metadata")
    for name in PATCHES:
        if receipt.get("patches", {}).get(name) != digest(Path(__file__).parent / name):
            raise ValueError("Kernel was built with different Iris patches")
    if digest(kernel_output / ARTIFACT) != receipt.get("artifact_sha256"):
        raise ValueError("Kernel artifact checksum mismatch")
    return receipt


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--kernel-output", type=Path, required=True)
    p.add_argument("--armada", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    receipt = validate(args.kernel_output)
    # Preserve Armada's own initramfs/splash/fallback validation, not a private
    # replacement for its boot image and OSTree finalization machinery.
    initramfs = subprocess.check_output([
        "git", "-C", str(args.armada), "show",
        f"{ARMADA}:build_files/55-generate-initramfs.sh"])
    args.output.mkdir(parents=True, exist_ok=False)
    out = args.output
    shutil.copyfile(args.kernel_output / ARTIFACT, out / ARTIFACT)
    shutil.copyfile(args.kernel_output / "build-receipt.json", out / "build-receipt.json")
    shutil.copyfile(Path(__file__).parent / "install-image-kernel.py", out / "install-image-kernel.py")
    (out / "55-generate-initramfs.sh").write_bytes(initramfs)
    (out / "Containerfile").write_text(f'''FROM {BASE}
LABEL org.traineros.kernel.candidate="{KERNEL}"
LABEL org.traineros.kernel.armada-source="{ARMADA}"
COPY {ARTIFACT} build-receipt.json install-image-kernel.py 55-generate-initramfs.sh /tmp/traineros-kernel/
RUN python3 /tmp/traineros-kernel/install-image-kernel.py && \\
    bash /tmp/traineros-kernel/55-generate-initramfs.sh && \\
    bootc container lint && \\
    rm -rf /tmp/traineros-kernel
''', encoding="utf-8", newline="\n")
    manifest = {"base_image": BASE, "armada_commit": ARMADA,
                "kernel_release": KERNEL, "artifact_sha256": receipt["artifact_sha256"],
                "status": "context-prepared-not-built", "installed": False,
                "files": {p.name: digest(p) for p in sorted(out.iterdir()) if p.is_file()}}
    (out / "image-context.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("IMAGE_CONTEXT_PREPARED_NOT_BUILT", out.resolve())


if __name__ == "__main__":
    main()
