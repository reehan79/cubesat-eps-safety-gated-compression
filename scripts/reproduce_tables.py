#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    verify = root / "scripts" / "verify_results.py"
    if not verify.is_file():
        print("FAIL: verify_results.py not found")
        return 1

    print("This lightweight reproducibility script verifies final published tables.")
    print("Full table regeneration from raw pipelines is intentionally out-of-scope here.")
    print("Running verification checks...\n")
    proc = subprocess.run([sys.executable, str(verify)], cwd=str(root))
    return int(proc.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
