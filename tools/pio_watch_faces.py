Import("env")
import subprocess
import sys
from pathlib import Path

root = Path(env["PROJECT_DIR"])
script = root / "src" / "features" / "clock" / "tools" / "watch_face_pack.py"
subprocess.check_call([sys.executable, str(script)], cwd=str(root))
