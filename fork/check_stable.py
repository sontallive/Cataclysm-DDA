#!/usr/bin/env python3
"""Reject an experimental base or unreviewed upstream changes before packaging."""

import argparse
import json
from pathlib import Path
import re
import subprocess


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-json", type=Path)
    args = parser.parse_args()
    pin = json.loads(Path("fork/stable.json").read_text())
    assert re.fullmatch(r"0\.[A-Z](?:-\d+)?", pin["tag"]), "Not a stable release tag"
    assert re.fullmatch(r"[0-9a-f]{40}", pin["commit"])
    assert re.fullmatch(r"[0-9a-f]{64}", pin["sha256"])
    assert re.fullmatch(r"cdda-linux-with-graphics-and-sounds-x64-[\w-]+\.tar\.gz", pin["asset"])
    assert git("rev-parse", pin["tag"] + "^{commit}") == pin["commit"], "Stable tag moved"
    subprocess.run(["git", "merge-base", "--is-ancestor", pin["commit"], "HEAD"], check=True)

    # Explicit integration surface. New hooks require a deliberate review here.
    hooks = {
        "src/main.cpp", "src/sdltiles.cpp", "src/input_context.h",
        "src/input_context.cpp", "src/input.cpp", "src/uilist.cpp",
        "src/npctalk.cpp", "src/mutation_ui.cpp", "src/bionics_ui.cpp", "src/debug.cpp",
    }
    modules = {
        "src/touch_shortcuts.cpp", "src/touch_shortcuts.h",
        "src/touch_shortcuts_context.cpp", "src/touch_shortcuts_context.h",
        "src/sdl_touch_shortcuts.cpp", "src/sdl_touch_shortcuts.h",
        "tests/touch_shortcuts_test.cpp",
        ".github/workflows/linux-release.yml", "FORK.md",
    }
    changed = git("diff", "--name-only", pin["commit"]).splitlines()
    unexpected = [p for p in changed if p not in hooks | modules and not p.startswith("fork/")]
    assert not unexpected, "Changes outside the stable fork patch surface: " + ", ".join(unexpected)

    if args.release_json:
        release = json.loads(args.release_json.read_text())
        assert release["tag_name"] == pin["tag"]
        assert not release["draft"] and not release["prerelease"], "Upstream release is not stable"
        asset = next(a for a in release["assets"] if a["name"] == pin["asset"])
        assert asset["digest"] == "sha256:" + pin["sha256"], "Official asset digest changed"
    print(f"Stable base verified: {pin['tag']} ({pin['commit']}); {len(changed)} fork files")


if __name__ == "__main__":
    main()
