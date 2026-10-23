#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Container build step only. Refuses to operate on an OSTree host."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile
import tempfile

KERNEL = "7.2.6-traineros-iris1"


def validate_member(member):
    path = PurePosixPath(member.name)
    expected = PurePosixPath("lib/modules") / KERNEL
    if path.is_absolute() or ".." in path.parts:
        raise ValueError("Unsafe kernel archive path")
    if path not in (PurePosixPath("lib"), PurePosixPath("lib/modules"), expected) and expected not in path.parents:
        raise ValueError("Archive contains files outside its kernel tree")
    if not (member.isdir() or member.isfile()):
        raise ValueError("Kernel archive must contain only directories and regular files")


def main():
    if Path("/run/ostree-booted").exists() or not Path("/run/.containerenv").exists():
        raise RuntimeError("Run only inside the bootc image build container")
    if Path("/usr/lib/armada/version").read_text().strip() != "20261017.64207e6":
        raise RuntimeError("Unexpected Armada base image")
    modules = Path("/usr/lib/modules")
    if sorted(p.name for p in modules.iterdir()) != ["7.2.6"] or (modules / "7.2.6").is_symlink():
        raise RuntimeError("Unexpected base kernel layout")
    root = Path(__file__).resolve().parent
    receipt = json.loads((root / "build-receipt.json").read_text())
    if receipt.get("status") != "built" or receipt.get("kernel_release") != KERNEL:
        raise RuntimeError("Incomplete kernel artifact")
    artifact = root / f"armada-kernel-{KERNEL}.tar.zst"
    with artifact.open("rb") as f:
        if hashlib.file_digest(f, "sha256").hexdigest() != receipt.get("artifact_sha256"):
            raise RuntimeError("Kernel checksum mismatch")
    # Validate and stage everything before removing the image's old tree. Never
    # extract an archive directly over /usr or trust its symlinks/devices.
    with tempfile.TemporaryDirectory(prefix="traineros-kernel-") as tmp:
        with tempfile.TemporaryFile() as decoded:
            subprocess.run(["zstd", "-dc", str(artifact)], stdout=decoded, check=True)
            decoded.seek(0)
            with tarfile.open(fileobj=decoded, mode="r:") as tar:
                for member in tar.getmembers():
                    validate_member(member)
                tar.extractall(tmp, filter="data")
        tree = Path(tmp) / "lib/modules" / KERNEL
        for name in ("vmlinuz", "modules.order", "modules.builtin", ".armada-source",
                     "kernel/drivers/media/platform/qcom/iris/qcom-iris.ko"):
            if not (tree / name).is_file() or not (tree / name).stat().st_size:
                raise RuntimeError(f"Missing full kernel output: {name}")
        if len(list((tree / "dtb/qcom").glob("*.dtb"))) != 27:
            raise RuntimeError("Incomplete Armada device tree set")
        shutil.copytree(tree, modules / KERNEL)
    shutil.rmtree(modules / "7.2.6")
    subprocess.run(["depmod", "-a", KERNEL, "-b", "/"], check=True)
    records = Path("/usr/lib/traineros/kernel")
    records.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / "build-receipt.json", records / "build-receipt.json")


if __name__ == "__main__":
    main()
