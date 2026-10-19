"""Build and run all QEMU-capable test apps under this project's tests directory.

Run with the ESP-IDF Python interpreter from an activated ESP-IDF terminal:
    python tools/run_qemu_tests.py
Each app opts in with CMakeLists.txt and sdkconfig.defaults.qemu. All pytest
files within that app are collected; no case names are stored in this runner.
"""

import os
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]


def run(command: list[str], cwd: Path = PROJECT) -> None:
    print(f"Running in {cwd}: {subprocess.list2cmdline(command)}", flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def main() -> int:
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        raise SystemExit("Activate the ESP-IDF environment before running this script.")
    apps = sorted(
        config.parent
        for config in (PROJECT / "tests").rglob("sdkconfig.defaults.qemu")
        if (config.parent / "CMakeLists.txt").is_file()
        and not any(part.startswith("build") for part in config.relative_to(PROJECT / "tests").parts)
    )
    if not apps:
        raise SystemExit("No QEMU test apps found; refusing an empty regression run.")
    failed = []
    for app in apps:
        name = app.relative_to(PROJECT / "tests").as_posix().replace("/", "-")
        build = app / "build-qemu"
        try:
            run(
                [
                    sys.executable,
                    str(Path(idf_path) / "tools/idf.py"),
                    "-C",
                    str(app),
                    "-B",
                    str(build),
                    "-DIDF_TARGET=esp32c3",
                    f"-DSDKCONFIG={build / 'sdkconfig'}",
                    "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.qemu",
                    "build",
                ]
            )
            run(
                [
                    "uv",
                    "run",
                    "--group",
                    "test",
                    "python",
                    "-m",
                    "esptool",
                    "--chip",
                    "esp32c3",
                    "merge-bin",
                    "--output",
                    "flash_image.bin",
                    "--pad-to-size",
                    "4MB",
                    "@flash_args",
                ],
                cwd=build,
            )
            run(
                [
                    "uv",
                    "run",
                    "--group",
                    "test",
                    "pytest",
                    str(app),
                    "--embedded-services",
                    "idf,qemu",
                    "--target",
                    "esp32c3",
                    "--app-path",
                    str(app),
                    "--build-dir",
                    "build-qemu",
                    "--skip-regenerate-image",
                    "yes",
                    f"--junitxml=test-results/{name}-qemu.xml",
                    f"--root-logdir=test-results/{name}-embedded",
                    "--unity-test-report-mode",
                    "merge",
                    "-v",
                ]
            )
        except (subprocess.CalledProcessError, OSError) as error:
            print(f"FAILED {name}: {error}", file=sys.stderr)
            failed.append(name)
    if failed:
        print(f"Regression failed: {', '.join(failed)}", file=sys.stderr)
        return 1
    print(f"Regression passed: {len(apps)} test app(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
