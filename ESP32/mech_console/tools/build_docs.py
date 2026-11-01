"""Build public API docs with a pinned, SHA-256-verified upstream Doxygen binary.

Run with `uv run python tools/build_docs.py`. The first run downloads Doxygen
into .cache/. Generated Pages content is written to build-docs/site/.
"""

import hashlib
import os
import platform
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

PROJECT = Path(__file__).resolve().parents[1]
VERSION = "1.18.0"
RELEASE = "https://github.com/doxygen/doxygen/releases/download/Release_1_18_0"
ARCHIVES = {
    "Windows": (
        "doxygen-1.18.0.windows.x64.bin.zip",
        "e84f54cfd49ef06b0b16536056dbec0c496323de28abcce53a4269463de35eaf",
        "doxygen.exe",
    ),
    "Linux": (
        "doxygen-1.18.0.linux.bin.tar.gz",
        "14fa81bdc34171edb5f1f02b1d60e74802f0439b77fa44e592565d517d72df90",
        "doxygen",
    ),
}


class LocalLinks(HTMLParser):
    """Collect local asset and navigation URLs from generated HTML."""

    def __init__(self) -> None:
        super().__init__()
        self.urls = []

    def handle_starttag(self, tag, attrs) -> None:
        self.urls.extend(value for name, value in attrs if name in {"href", "src"} and value)


def check_links(site: Path) -> None:
    broken = []
    for page in site.rglob("*.html"):
        parser = LocalLinks()
        parser.feed(page.read_text(encoding="utf-8"))
        for url in parser.urls:
            parts = urlsplit(url)
            if parts.scheme or parts.netloc or not parts.path:
                continue
            target = page.parent / unquote(parts.path)
            if target.is_dir():
                target /= "index.html"
            if not target.is_file():
                broken.append(f"{page.relative_to(site)}: {url}")
    if broken:
        raise SystemExit("Broken documentation links:\n" + "\n".join(broken))


def doxygen() -> Path:
    system = platform.system()
    if system not in ARCHIVES or platform.machine().lower() not in {"amd64", "x86_64"}:
        raise SystemExit("The pinned binary supports Windows/Linux x64 only.")
    filename, digest, executable = ARCHIVES[system]
    cache = PROJECT / ".cache" / "doxygen" / VERSION / system
    binaries = sorted(cache.rglob(executable)) if cache.exists() else []
    if not binaries:
        cache.mkdir(parents=True, exist_ok=True)
        archive = cache / filename
        print(f"Downloading Doxygen {VERSION} for {system}...", flush=True)
        request = urllib.request.Request(f"{RELEASE}/{filename}", headers={"User-Agent": "mech-console-docs"})
        with urllib.request.urlopen(request, timeout=120) as response, archive.open("wb") as destination:
            shutil.copyfileobj(response, destination)
        with archive.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        if actual != digest:
            raise SystemExit(f"Doxygen archive checksum mismatch: {actual}")
        if system == "Windows":
            with zipfile.ZipFile(archive) as bundle:
                bundle.extractall(cache)
        else:
            with tarfile.open(archive) as bundle:
                bundle.extractall(cache, filter="data")
        binaries = sorted(cache.rglob(executable))
    if len(binaries) != 1:
        raise SystemExit(f"Expected one Doxygen executable in {cache}, found {len(binaries)}")
    binary = binaries[0]
    if system == "Linux":
        binary.chmod(binary.stat().st_mode | 0o111)
    actual_version = subprocess.check_output([str(binary), "--version"], text=True).strip().split()[0]
    if actual_version != VERSION:
        raise SystemExit(f"Expected Doxygen {VERSION}, found {actual_version}")
    return binary


def main() -> int:
    binary = doxygen()
    headers = sorted(PROJECT.glob("components/*/include/**/*.h"))
    if not headers:
        raise SystemExit("No public component headers found.")

    # Resolve and verify the exact generated directory before removing it.
    output = (PROJECT / "build-docs").resolve()
    if output != PROJECT / "build-docs":
        raise SystemExit("Refusing to clean a redirected documentation directory.")
    if output.exists():
        shutil.rmtree(output)
    site = output / "site"
    (site / "ESP32" / "mech_console").mkdir(parents=True)

    config = (PROJECT / "Doxyfile").read_text(encoding="utf-8")
    config += "\nINPUT += " + " ".join(f'"{path.relative_to(PROJECT).as_posix()}"' for path in headers) + "\n"
    commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=PROJECT, text=True).strip()
    config += f'PROJECT_NUMBER = "{commit}"\n'
    environment = os.environ.copy()
    environment["PYTHONUTF8"] = "1"
    subprocess.run([str(binary), "-"], input=config, text=True, cwd=PROJECT, env=environment, check=True)

    (site / ".nojekyll").touch()
    (site / "index.html").write_text(
        '<!doctype html><html lang="en"><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width, initial-scale=1">'
        "<title>Embedded Applications documentation</title>"
        "<h1>Embedded Applications</h1><ul>"
        '<li><a href="ESP32/mech_console/">ESP32 / mech_console API</a></li>'
        "</ul></html>\n",
        encoding="utf-8",
    )
    check_links(site)
    print(f"Built {len(headers)} public headers: {site / 'ESP32/mech_console/index.html'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
