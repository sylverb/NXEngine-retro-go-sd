#!/usr/bin/env python3
"""Download freeware Cave Story and prepare the CaveStory/ tree for pack-assets.

Steps:
  1. Fetch https://www.cavestory.one/downloads/cavestoryen.zip (or use --zip)
  2. Unzip into <outdir>/ (expects a top-level CaveStory/ folder)
  3. Extract org/pxt/endpic/wavetable/stage.dat from Doukutsu.exe
  4. Copy NXEngine support files (sprites.sif, fonts, tilekey.dat)

Audio caches (drum.pcm / sndcache.pcm) are built separately by the host
binary:  ./CaveStory_host --ci-prepare

Usage:
  python3 scripts/prepare_cavestory_tree.py
  python3 scripts/prepare_cavestory_tree.py --zip /tmp/cavestoryen.zip
"""
from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_URL = "https://www.cavestory.one/downloads/cavestoryen.zip"
ENGINE_FILES = (
    "sprites.sif",
    "tilekey.dat",
    "smalfont.bmp",
    "font.ttf",
)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_doukutsu import (  # noqa: E402
    extract_files,
    extract_pxt,
    extract_stages,
)


def download(url: str, dest: Path) -> None:
    print(f"downloading {url}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as resp, open(dest, "wb") as out:
        shutil.copyfileobj(resp, out)
    print(f"  → {dest} ({dest.stat().st_size} bytes)")


def unzip_to(zip_path: Path, dest: Path) -> Path:
    print(f"unzip {zip_path} → {dest}")
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path) as zf:
        zf.extractall(dest)
    cand = dest / "CaveStory"
    if cand.is_dir() and (cand / "Doukutsu.exe").is_file():
        return cand
    # Some mirrors may flatten the archive.
    exe = next(dest.rglob("Doukutsu.exe"), None)
    if exe is None:
        raise SystemExit("Doukutsu.exe not found after unzip")
    return exe.parent


def copy_engine_files(tree: Path) -> None:
    eng = ROOT / "third_party" / "nxengine"
    for name in ENGINE_FILES:
        src = eng / name
        if not src.is_file():
            raise SystemExit(f"missing engine support file: {src}")
        dst = tree / name
        shutil.copy2(src, dst)
        print(f"  [engine] {name}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--url",
        default=DEFAULT_URL,
        help=f"download URL (default: {DEFAULT_URL})",
    )
    ap.add_argument(
        "--zip",
        type=Path,
        default=None,
        help="use an already-downloaded cavestoryen.zip instead of downloading",
    )
    ap.add_argument(
        "--outdir",
        type=Path,
        default=ROOT,
        help="directory that will contain CaveStory/ (default: repo root)",
    )
    ap.add_argument(
        "--keep-zip",
        action="store_true",
        help="keep the downloaded zip under outdir/",
    )
    args = ap.parse_args()

    outdir = args.outdir.resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    tmp_dir = None
    try:
        if args.zip:
            zip_path = args.zip.resolve()
            if not zip_path.is_file():
                print(f"error: zip not found: {zip_path}", file=sys.stderr)
                return 1
        else:
            if args.keep_zip:
                zip_path = outdir / "cavestoryen.zip"
            else:
                tmp_dir = tempfile.TemporaryDirectory(prefix="cavestoryen-")
                zip_path = Path(tmp_dir.name) / "cavestoryen.zip"
            download(args.url, zip_path)

        tree = unzip_to(zip_path, outdir)
        # Ensure canonical name CaveStory/ at outdir.
        canonical = outdir / "CaveStory"
        if tree.resolve() != canonical.resolve():
            if canonical.exists():
                shutil.rmtree(canonical)
            shutil.move(str(tree), str(canonical))
            tree = canonical

        exe = tree / "Doukutsu.exe"
        print(f"extracting from {exe}")
        blob = exe.read_bytes()
        extract_files(blob, tree)
        extract_pxt(blob, tree)
        extract_stages(blob, tree)
        copy_engine_files(tree)
        print(f"Cave Story tree ready at {tree}")
        print("Next: build host audio caches with  ./CaveStory_host --ci-prepare")
        return 0
    finally:
        if tmp_dir is not None:
            tmp_dir.cleanup()


if __name__ == "__main__":
    sys.exit(main())
