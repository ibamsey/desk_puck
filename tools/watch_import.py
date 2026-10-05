#!/usr/bin/env python3
"""Shim: run WatchMaker import from repo root."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
script = ROOT / "src" / "features" / "clock" / "tools" / "watch_import.py"
raise SystemExit(subprocess.call([sys.executable, str(script), *sys.argv[1:]], cwd=str(ROOT)))
