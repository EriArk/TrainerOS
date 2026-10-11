#!/usr/bin/env python3
"""Complete an uninstalled Fedora/Armada RMG bundle, before fingerprinting it.

Run in the matching Fedora build container. Base Qt/SDL/Vulkan/glibc libraries
come from the supported host image; only the three missing libraries are copied.
This is not a general AppImage packager or a host package installer.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


LIBRARIES = ("libminizip.so.1", "libSDL3_net.so.0", "libspeexdsp.so.1")


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def elf_machine(path):
    with path.open("rb") as stream:
        header = stream.read(20)
    if len(header) != 20 or header[:4] != b"\x7fELF" or header[5] not in (1, 2):
        raise ValueError(f"Not an ELF file: {path}")
    return int.from_bytes(header[18:20], "little" if header[5] == 1 else "big")


def package(root, library_directory):
    executable = root / "bin/RMG"
    machine = elf_machine(executable)
    destination = root / "lib"
    marker = root / "dependency-provenance.json"
    if marker.exists() or (root / "manifest.json").exists() or any((destination / name).exists() for name in LIBRARIES):
        raise ValueError("Use an uncompleted staging bundle; retain the installed version")
    if not shutil.which("patchelf"):
        raise ValueError("Install patchelf in the build container first")
    records = []
    # Resolve and validate the complete source set before modifying the bundle.
    for name in LIBRARIES:
        source = (library_directory / name).resolve(strict=True)
        if elf_machine(source) != machine or run("patchelf", "--print-soname", str(source)) != name:
            raise ValueError(f"Wrong architecture/SONAME for {name}")
        owner = run("rpm", "-qf", "--qf", "%{NAME}", str(source))
        licenses = [Path(p) for p in run("rpm", "-q", "--licensefiles", owner).splitlines() if Path(p).is_file()]
        # Fedora's compat subpackage omits its license file. Keep the exact
        # upstream notice with the recipe, and fail closed on a version change.
        if not licenses and owner == "minizip-ng-compat" and run("rpm", "-q", "--qf", "%{VERSION}", owner) == "4.1.0":
            notice = Path(__file__).resolve().with_name("LICENSE.minizip-ng-4.1.0")
            if hashlib.sha256(notice.read_bytes()).hexdigest() != "675181c03fc1302a1c8554c00f7be9bb420c5dbc9dcc2013433cec144413de03":
                raise ValueError("Changed minizip license notice")
            licenses = [notice]
        if not licenses:
            raise ValueError(f"Missing license files for {owner}")
        records.append((name, source, owner, licenses))
    provenance = []
    for name, source, owner, licenses in records:
        shutil.copy2(source, destination / name)
        license_root = root / "licenses/dependencies" / owner
        for license_file in licenses:
            # Preserve subdirectories instead of merging same-named notices.
            target = license_root / license_file.relative_to("/")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(license_file, target)
        provenance.append({"library": name, "package": run("rpm", "-q", owner),
                           "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                           "licenses": [str(p.relative_to("/")) for p in licenses]})
    # Plugins live several levels below lib: a single $ORIGIN/../lib would fail
    # for their transitive dependencies. No absolute build paths survive here.
    for paths in ([executable], sorted(destination.rglob("*"))):
        for path in paths:
            if not path.is_file() or path.is_symlink():
                continue
            with path.open("rb") as stream:
                if stream.read(4) != b"\x7fELF":
                    continue
            relative = os.path.relpath(destination, path.parent)
            rpath = "$ORIGIN" if relative == "." else "$ORIGIN/" + relative
            run("patchelf", "--set-rpath", rpath, str(path))
    marker.write_text(json.dumps({"protocol": 1, "elfMachine": machine, "libraries": provenance}, indent=2) + "\n")
    print(f"Completed {len(provenance)} dependency libraries; verify ldd on each target host before installation.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--library-directory", type=Path, default=Path("/usr/lib64"))
    args = parser.parse_args()
    package(args.bundle.resolve(strict=True), args.library_directory.resolve(strict=True))
