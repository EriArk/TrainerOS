#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the pinned diagnostic Iris module; never install or load it.

Run on ARM64 with the matching kernel compiler, make, binutils, flex, bison,
bc, elfutils development headers, OpenSSL development headers and patch.
The output directory must not exist. See README.md before loading any artifact.
"""
import argparse
import hashlib
import io
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

ARMADA = "fd0ed0f1a285fbbccc5fa63969d4e176cb0ae4e6"
LINUX_SHA256 = "039aef84f2b0994aeda3f4fcfc3d02ec9d7a9bbb9020ea264c43f446c860f606"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linux-tarball", type=Path, required=True)
    parser.add_argument("--armada", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data = args.linux_tarball.read_bytes()
    if hashlib.sha256(data).hexdigest() != LINUX_SHA256:
        parser.error("Expected the pinned Linux 7.2.6 archive")
    config = args.config.read_text()
    if "# CONFIG_MODVERSIONS is not set" not in config:
        parser.error("This module-only procedure requires CONFIG_MODVERSIONS disabled")
    if 'CONFIG_CC_VERSION_TEXT="gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)"' not in config:
        parser.error("Expected the verified Armada kernel configuration")
    compiler = subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0]
    if compiler != "gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)":
        parser.error("Compiler does not match the verified device kernel")
    # Archive the pinned commit, not mutable files in a developer's checkout.
    archive = subprocess.check_output([
        "git", "-C", str(args.armada), "archive", ARMADA, "packages/kernel"])
    args.output.mkdir(parents=True, exist_ok=False)
    root = args.output.resolve()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:xz") as tar:
        tar.extractall(root, filter="data")
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as tar:
        tar.extractall(root, filter="data")
    source = root / "linux-7.2.6"
    patches = root / "packages/kernel/patches"
    series = [line.split("#")[0].strip() for line in (patches / "series").read_text().splitlines()]
    selected = [patches / name for name in series if name]
    if len(selected) != 196:
        raise RuntimeError("Unexpected Armada patch series")
    selected.extend(Path(__file__).resolve().parent / name for name in (
        "0001-upstream-request-keyframe.patch", "0002-gen1-visible-input-size.patch"))
    for patch in selected:
        payload = patch.read_bytes()
        check = subprocess.run(["patch", "-p1", "--batch", "--forward", "-F0",
                                "--dry-run", "--verbose"], cwd=source,
                               input=payload, capture_output=True, check=True)
        expected = len(re.findall(br"^@@ ", payload, re.M))
        actual = len(re.findall(br"^Hunk #[0-9]+ succeeded", check.stdout, re.M))
        if not expected or actual != expected:
            raise RuntimeError(f"Incomplete patch application: {patch.name}")
        subprocess.run(["patch", "-p1", "--batch", "--forward", "-F0",
                        "--no-backup-if-mismatch", "--quiet"],
                       cwd=source, input=payload, check=True)
        print("Applied", patch.name, flush=True)
    shutil.copyfile(args.config, source / ".config")
    subprocess.run(["make", "olddefconfig"], cwd=source, check=True)
    before = {line for line in config.splitlines() if line.startswith(("CONFIG_", "# CONFIG_"))}
    after = {line for line in (source / ".config").read_text().splitlines()
             if line.startswith(("CONFIG_", "# CONFIG_"))}
    if (before ^ after) - {"CONFIG_OPENSSL_SUPPORTS_ML_DSA=y"}:
        raise RuntimeError("Kernel configuration drift; do not build this module")
    subprocess.run(["make", "-j2", "modules_prepare"], cwd=source, check=True)
    # No original Module.symvers: load only after the documented export/ABI checks.
    subprocess.run(["make", "-j2", "KBUILD_MODPOST_WARN=1",
                    "M=drivers/media/platform/qcom/iris", "modules"], cwd=source, check=True)
    module = source / "drivers/media/platform/qcom/iris/qcom-iris.ko"
    print("Built", module)
    print("SHA256", hashlib.sha256(module.read_bytes()).hexdigest())
    print("NOT INSTALLED. Perform the README ABI/export checks before temporary loading.")


if __name__ == "__main__":
    main()
