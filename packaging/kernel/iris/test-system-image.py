#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-only image admission tests. Never operates on a device or /usr."""
import importlib.util
import json
from pathlib import Path
import tarfile
import tempfile
import unittest


def load(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).parent / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


prepare = load("prepare-system-image")
install = load("install-image-kernel")


class ImageAdmission(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.artifact = self.root / prepare.ARTIFACT
        self.artifact.write_bytes(b"test fixture, not a kernel archive")
        self.receipt = {"status": "built", "armada_commit": prepare.ARMADA,
                        "kernel_release": prepare.KERNEL, "artifact": prepare.ARTIFACT,
                        "module_symvers_sha256": "fixture", "config_sha256": "fixture",
                        "artifact_sha256": prepare.digest(self.artifact),
                        "patches": {name: prepare.digest(Path(__file__).parent / name)
                                    for name in prepare.PATCHES}}

    def validate(self):
        (self.root / "build-receipt.json").write_text(json.dumps(self.receipt))
        return prepare.validate(self.root)

    def test_matching_receipt_is_admitted(self):
        self.assertEqual(self.validate(), self.receipt)

    def test_incomplete_or_different_build_rejected(self):
        for field, bad in (("status", "prepared"), ("armada_commit", "other"),
                           ("kernel_release", "7.2.6"), ("artifact", "../../kernel"),
                           ("module_symvers_sha256", ""), ("config_sha256", "")):
            with self.subTest(field=field):
                saved = self.receipt[field]
                self.receipt[field] = bad
                with self.assertRaises(ValueError):
                    self.validate()
                self.receipt[field] = saved

    def test_changed_patch_rejected(self):
        self.receipt["patches"][prepare.PATCHES[0]] = "wrong"
        with self.assertRaises(ValueError):
            self.validate()

    def test_corrupted_artifact_rejected(self):
        self.artifact.write_bytes(b"changed in transit")
        with self.assertRaises(ValueError):
            self.validate()

    def test_only_selected_module_tree_admitted(self):
        for name in ("lib", "lib/modules", "lib/modules/" + install.KERNEL,
                     "lib/modules/" + install.KERNEL + "/kernel/qcom-iris.ko"):
            with self.subTest(name=name):
                install.validate_member(tarfile.TarInfo(name))

    def test_archive_cannot_replace_foreign_paths(self):
        for name in ("/etc/passwd", "lib/modules/../../etc/passwd", "lib/modules/7.2.6/kernel/x.ko",
                     "usr/lib/modules/" + install.KERNEL + "/x.ko", "lib/modules-other/x"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                install.validate_member(tarfile.TarInfo(name))

    def test_archive_cannot_redirect_extraction(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.CHRTYPE, tarfile.BLKTYPE, tarfile.FIFOTYPE):
            member = tarfile.TarInfo("lib/modules/" + install.KERNEL + "/redirect")
            member.type = kind
            member.linkname = "/usr"
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                install.validate_member(member)


if __name__ == "__main__":
    unittest.main()
