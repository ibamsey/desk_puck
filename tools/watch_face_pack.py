#!/usr/bin/env python3
"""Shim: run the clock feature packer from repo root."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
script = ROOT / "src" / "features" / "clock" / "tools" / "watch_face_pack.py"
raise SystemExit(subprocess.call([sys.executable, str(script)], cwd=str(ROOT)))
