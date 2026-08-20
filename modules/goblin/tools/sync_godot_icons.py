#!/usr/bin/env python
"""Sync Godot's built-in editor icons into modules/goblin for easy rebranding.

Goal:
- Copy all `editor/icons/*.svg` into `modules/goblin/editor/icons/`.
- Do NOT modify Godot's original files.
- Existing files in `modules/goblin/editor/icons` are left untouched.

Usage (repo root):
  python modules/goblin/tools/sync_godot_icons.py

Optional:
  python modules/goblin/tools/sync_godot_icons.py --force   # overwrite existing goblin copies
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--force", action="store_true", help="Overwrite existing files in modules/goblin/editor/icons")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[3]
    src_dir = repo_root / "editor" / "icons"
    dst_dir = repo_root / "modules" / "goblin" / "editor" / "icons"

    if not src_dir.is_dir():
        raise SystemExit(f"Missing source icons dir: {src_dir}")

    dst_dir.mkdir(parents=True, exist_ok=True)

    copied = 0
    skipped = 0
    for src in sorted(src_dir.glob("*.svg")):
        dst = dst_dir / src.name
        if dst.exists() and not args.force:
            skipped += 1
            continue
        shutil.copy2(src, dst)
        copied += 1

    print(f"Goblin: synced editor icons -> {dst_dir}")
    print(f"  copied:  {copied}")
    print(f"  skipped: {skipped}")
    print("\nNext: replace any SVGs in modules/goblin/editor/icons with your Goblin versions.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
