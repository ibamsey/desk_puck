#!/usr/bin/env python3
"""Regenerate bell-ross from source/*.watch (see also tools/watch_import.py)."""

from __future__ import annotations

import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))
from _paths import face_dir  # noqa: E402
from watch_import_lib import import_watch_file  # noqa: E402

SLUG = "bell-ross"
SOURCE = face_dir(SLUG) / "source"
WATCH_NAME = "bell--ross-br-s-white-ceramic.watch"


def main() -> None:
    watch_path = SOURCE / WATCH_NAME
    if not watch_path.is_file():
        raise SystemExit(f"Missing {watch_path}")
    result = import_watch_file(
        watch_path,
        face_dir(SLUG),
        SLUG,
        "Bell & Ross BR S",
        force=True,
    )
    for note in result.notes:
        print(f"  note: {note}")
    print(f"Generated {SLUG}")


if __name__ == "__main__":
    main()
