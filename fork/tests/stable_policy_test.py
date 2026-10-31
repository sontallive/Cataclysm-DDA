#!/usr/bin/env python3
"""Exercise stable-only build refusals in a disposable Git repository."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

CHECK = Path(__file__).resolve().parents[1] / "check_stable.py"


class StablePolicyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "-q", "--allow-empty", "-m", "Official stable base")
        self.base = self.git("rev-parse", "HEAD").strip()
        self.git("tag", "0.I-1")
        (self.root / "fork").mkdir()
        self.pin = {"tag": "0.I-1", "commit": self.base,
                    "asset": "cdda-linux-with-graphics-and-sounds-x64-test.tar.gz",
                    "sha256": "a" * 64}
        self.release = {"tag_name": "0.I-1", "draft": False, "prerelease": False,
                        "assets": [{"name": self.pin["asset"], "digest": "sha256:" + "a" * 64}]}

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True)

    def check(self):
        (self.root / "fork/stable.json").write_text(json.dumps(self.pin))
        (self.root / "fork/release.json").write_text(json.dumps(self.release))
        return subprocess.run(["python3", str(CHECK), "--release-json", "fork/release.json"],
                              cwd=self.root, capture_output=True, text=True).returncode

    def test_accepts_pinned_stable(self):
        self.assertEqual(self.check(), 0)

    def test_rejects_experimental_tag(self):
        self.pin["tag"] = "cdda-experimental-2026-10-31-0000"
        self.assertNotEqual(self.check(), 0)

    def test_rejects_prerelease_and_draft(self):
        for field in ("prerelease", "draft"):
            with self.subTest(field=field):
                self.release[field] = True
                self.assertNotEqual(self.check(), 0)
                self.release[field] = False

    def test_rejects_wrong_commit(self):
        self.pin["commit"] = "f" * 40
        self.assertNotEqual(self.check(), 0)

    def test_rejects_changed_resource_digest(self):
        self.release["assets"][0]["digest"] = "sha256:" + "b" * 64
        self.assertNotEqual(self.check(), 0)

    def test_rejects_changes_outside_patch_surface(self):
        (self.root / "gameplay.json").write_text("{}")
        self.git("add", "gameplay.json")
        self.assertNotEqual(self.check(), 0)


if __name__ == "__main__":
    unittest.main()
