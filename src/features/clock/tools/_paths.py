"""Shared paths for clock feature build scripts."""

from pathlib import Path

CLOCK = Path(__file__).resolve().parents[1]
ASSETS_FACES = CLOCK / "assets" / "faces"
GENERATED = CLOCK / "generated"


def face_dir(slug: str) -> Path:
    return ASSETS_FACES / slug
