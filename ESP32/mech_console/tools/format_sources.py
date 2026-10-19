"""Format project-owned C/H/Python sources, or verify them with --check.

Usage: uv run --group dev python tools/format_sources.py [--check]
Git's file list excludes generated, ignored, and third-party build contents.
"""

import argparse
import subprocess
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]


def run(command: list[str]) -> int:
    return subprocess.run(command, cwd=PROJECT, check=False).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report differences without changing files")
    args = parser.parse_args()
    listing = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=PROJECT
    )
    paths = sorted({name.decode("utf-8") for name in listing.split(b"\0") if name})
    c_files = [name for name in paths if Path(name).suffix in {".c", ".h"} and (PROJECT / name).is_file()]
    py_files = [name for name in paths if Path(name).suffix == ".py" and (PROJECT / name).is_file()]
    if not c_files or not py_files:
        raise SystemExit("Expected both C/H and Python sources; refusing an incomplete formatting run.")
    failed = False
    clang_args = ["--dry-run", "--Werror"] if args.check else ["-i"]
    # Small batches also fit Windows command-line length limits.
    for offset in range(0, len(c_files), 25):
        failed |= run(["clang-format", "--style=file", *clang_args, *c_files[offset : offset + 25]]) != 0
    ruff_args = [] if args.check else ["--fix"]
    failed |= run(["ruff", "check", *ruff_args, *py_files]) != 0
    failed |= run(["ruff", "format", *(["--check"] if args.check else []), *py_files]) != 0
    print(f"{'Checked' if args.check else 'Formatted'} {len(c_files)} C/H and {len(py_files)} Python files.")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
