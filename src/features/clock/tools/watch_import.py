#!/usr/bin/env python3
"""Import WatchMaker .watch files from watch_files/ into asset faces."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

try:
    from PIL import Image  # noqa: F401 — dependency check
except ImportError:
    print("watch_import: install Pillow (pip install pillow)", file=sys.stderr)
    sys.exit(1)

from _paths import ASSETS_FACES, CLOCK
from watch_import_lib import import_watch_file, slugify, watch_root_from_zip

REPO_ROOT = CLOCK.parents[2]
WATCH_FILES_DIR = REPO_ROOT / "watch_files"


def display_name_from_watch(path: Path, slug: str) -> str:
    try:
        with zipfile.ZipFile(path) as zf:
            root = watch_root_from_zip(zf)
            name = (root.attrib.get("name") or "").strip()
            if name:
                return name
    except (OSError, KeyError, ET.ParseError):
        pass
    return slug.replace("-", " ").title()


def collect_inputs(args: argparse.Namespace) -> list[Path]:
    if args.watch:
        p = Path(args.watch).resolve()
        if not p.is_file():
            raise SystemExit(f"Not found: {p}")
        return [p]
    if not WATCH_FILES_DIR.is_dir():
        WATCH_FILES_DIR.mkdir(parents=True, exist_ok=True)
        raise SystemExit(
            f"No .watch files given. Drop archives in {WATCH_FILES_DIR} or pass a path."
        )
    files = sorted(WATCH_FILES_DIR.glob("*.watch"))
    if not files:
        raise SystemExit(f"No *.watch in {WATCH_FILES_DIR}")
    return files


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert WatchMaker .watch → src/features/clock/assets/faces/<slug>/"
    )
    parser.add_argument(
        "watch",
        nargs="?",
        type=Path,
        help="Path to .watch (default: all files in watch_files/)",
    )
    parser.add_argument("--slug", help="Face folder name (default: from filename or watch name)")
    parser.add_argument("--name", help="Display name in face.json")
    parser.add_argument("--force", action="store_true", help="Overwrite existing face folder")
    parser.add_argument("--no-pack", action="store_true", help="Skip watch_face_pack.py")
    args = parser.parse_args()

    paths = collect_inputs(args)
    if args.slug and len(paths) > 1:
        print("warning: --slug ignored when importing multiple files", file=sys.stderr)
    for watch_path in paths:
        slug = args.slug if (args.slug and len(paths) == 1) else slugify(watch_path.stem)
        if not re.fullmatch(r"[a-z0-9-]+", slug):
            raise SystemExit(f"Invalid slug {slug!r} for {watch_path.name}; use --slug")

        display_name = args.name if (args.name and len(paths) == 1) else display_name_from_watch(
            watch_path, slug
        )
        out_dir = ASSETS_FACES / slug
        print(f"Importing {watch_path.name} -> {out_dir.relative_to(REPO_ROOT)}")
        result = import_watch_file(watch_path, out_dir, slug, display_name, args.force)
        for note in result.notes:
            print(f"  note: {note}")

    if not args.no_pack:
        packer = CLOCK / "tools" / "watch_face_pack.py"
        print("Running watch_face_pack.py...")
        rc = subprocess.call([sys.executable, str(packer)], cwd=str(REPO_ROOT))
        if rc != 0:
            return rc

    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
