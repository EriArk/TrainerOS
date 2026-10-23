#!/usr/bin/env python3
"""Create a separate local Flatpak branch; never install or overwrite stable.

The base supplies the already installed SDL2/libraries/assets and their licences.
Only PPSSPPSDL changes. See docs/emulators/ppsspp.md for review and rollback.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

BASE_HASH = "c99cec693067f8c94ae0d8b5e7299392ed5c0cfae3f8231176ed25ea6c155dfc"
BASE_REV = "fa50bb1976065c4f8b1b47af227d367fe9771555"
RELAY_REV = "572de778fabcbda819ab0f677368c3fbc3033e19"
APP = "org.ppsspp.PPSSPP"
BRANCH = "traineros-relay"


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path, help="Successful build.sh work directory")
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    source = work / "source"
    recipe = Path(__file__).resolve().parent
    binary = work / "build/PPSSPPSDL"
    assert run("git", "-C", str(source), "rev-parse", "HEAD") == BASE_REV
    assert run("git", "-C", str(source / "ext/aemu_postoffice"), "rev-parse", "HEAD") == RELAY_REV
    subprocess.run(["git", "-C", str(source), "apply", "--reverse", "--check",
                    str(recipe / "pdp-stat.patch")], check=True)
    subprocess.run(["git", "-C", str(source), "apply", "--reverse", "--check",
                    str(recipe / "relay-retry.patch")], check=True)
    subprocess.run(["git", "-C", str(source / "ext/aemu_postoffice"), "apply", "--reverse", "--check",
                    str(recipe / "socket-timeout.patch")], check=True)
    base = Path(run("flatpak", "info", "--system", "--show-location",
                    f"app/{APP}/aarch64/stable"))
    assert digest(base / "files/bin/PPSSPPSDL") == BASE_HASH, "Unreviewed base package"
    assert binary.is_file(), "Build the reviewed candidate first"
    stage = Path(tempfile.mkdtemp(prefix="package-", dir=work))
    shutil.copytree(base / "files", stage / "files", symlinks=True)
    shutil.copyfile(base / "metadata", stage / "metadata")
    if (base / "export").exists():
        shutil.copytree(base / "export", stage / "export", symlinks=True)
    target = stage / "files/bin/PPSSPPSDL"
    target.unlink()  # Remove only our private copied binary, never the OSTree file.
    shutil.copy2(binary, target)
    evidence = stage / "files/share/traineros-relay"
    evidence.mkdir(parents=True)
    for name in ("build.sh", "pdp-stat.patch", "relay-receive-test.c", "package.py",
                 "relay-retry.patch", "socket-timeout.patch", "relay-timeout-test.c", "relay-retry-test.cpp"):
        shutil.copy2(recipe / name, evidence / name)
    licence = recipe / "LICENSE"
    if not licence.is_file():
        licence = recipe.parents[1] / "LICENSE"
    shutil.copy2(licence, evidence / "LICENSE.TrainerOS")
    (evidence / "provenance.json").write_text(json.dumps({
        "basePackageCommit": base.name,
        "baseBinarySha256": BASE_HASH,
        "binarySha256": digest(binary),
        "ppssppRevision": BASE_REV,
        "relayRevision": RELAY_REV,
        "sdk": "org.freedesktop.Sdk/aarch64/25.08",
        "branch": BRANCH,
        "patchSha256": digest(recipe / "pdp-stat.patch"),
        "retryPatchSha256": digest(recipe / "relay-retry.patch"),
        "timeoutPatchSha256": digest(recipe / "socket-timeout.patch"),
    }, indent=2) + "\n", encoding="utf-8")
    repo = work / "repo"
    subprocess.run(["flatpak", "build-export", "--arch=aarch64",
                    str(repo), str(stage), BRANCH], check=True)
    bundle = work / "traineros-ppsspp-relay.flatpak"
    assert not bundle.exists(), "Keep existing bundles; move the prior artifact before rebuilding"
    subprocess.run(["flatpak", "build-bundle", "--arch=aarch64", str(repo),
                    str(bundle), APP, BRANCH], check=True)
    print(json.dumps({"bundle": str(bundle), "sha256": digest(bundle),
                      "binarySha256": digest(binary)}))


if __name__ == "__main__":
    main()
