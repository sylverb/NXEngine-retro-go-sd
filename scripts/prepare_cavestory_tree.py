#!/usr/bin/env python3
"""Download freeware Cave Story and prepare CaveStory/ for pack-assets.

English (`--locale en`, default):
  1. Fetch cavestoryen.zip
  2. Unzip → CaveStory/ with Doukutsu.exe
  3. Extract org/pxt/endpic/wavetable/stage.dat from Doukutsu.exe
  4. Copy NXEngine support files (sprites.sif, fonts, tilekey.dat)

Fan translations (`--locale fr`, …) are usually data overlays:
  1. Prepare English base as above
  2. Download the locale archive and merge its `data/` (and siblings) over
     CaveStory/, keeping Doukutsu.exe + extracted music/SFX assets

Audio caches (drum.pcm / sndcache.pcm): ./CaveStory_host --ci-prepare

Usage:
  python3 scripts/prepare_cavestory_tree.py
  python3 scripts/prepare_cavestory_tree.py --locale fr
  python3 scripts/prepare_cavestory_tree.py --list-locales
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE_FILES = (
    "sprites.sif",
    "tilekey.dat",
    "smalfont.bmp",
    "font.ttf",
)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cavestory_locales import get_locale, list_locales  # noqa: E402
from extract_doukutsu import (  # noqa: E402
    extract_files,
    extract_pxt,
    extract_stages,
)


def download(url: str, dest: Path) -> None:
    print(f"downloading {url}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "NXEngine-retro-go-sd/1.0"})
    with urllib.request.urlopen(req) as resp, open(dest, "wb") as out:
        shutil.copyfileobj(resp, out)
    print(f"  → {dest} ({dest.stat().st_size} bytes)")


def _find_7z() -> str | None:
    for name in ("7z", "7zz", "7za"):
        path = shutil.which(name)
        if path:
            return path
    return None


def extract_archive(archive: Path, dest: Path) -> None:
    """Extract .zip / .7z / NSIS .exe into dest (must exist)."""
    dest.mkdir(parents=True, exist_ok=True)
    suffix = archive.suffix.lower()
    print(f"extract {archive.name} → {dest}")

    if suffix == ".zip":
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
        # Nested NSIS installer (e.g. Windows FR zip → csfrsetup.exe)
        for exe in dest.rglob("*.exe"):
            if exe.stat().st_size < 200_000:
                continue
            # Heuristic: Nullsoft self-extractors are often the only large exe
            seven = _find_7z()
            if seven is None:
                break
            try:
                probe = subprocess.run(
                    [seven, "t", str(exe)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if probe.returncode == 0 and "Nsis" in (probe.stdout + probe.stderr):
                    print(f"  unpacking NSIS installer {exe.name}")
                    subprocess.run(
                        [seven, "x", f"-o{dest}", "-y", str(exe)],
                        check=True,
                        capture_output=True,
                    )
            except OSError:
                pass
        return

    seven = _find_7z()
    if seven is None:
        raise SystemExit(
            f"need p7zip (`7z`) to extract {archive.suffix}; "
            "install p7zip-full (apt) or p7zip (brew)"
        )
    subprocess.run(
        [seven, "x", f"-o{dest}", "-y", str(archive)],
        check=True,
        capture_output=True,
    )


def find_doukutsu_tree(root: Path) -> Path:
    cand = root / "CaveStory"
    if cand.is_dir() and (cand / "Doukutsu.exe").is_file():
        return cand
    exe = next(root.rglob("Doukutsu.exe"), None)
    if exe is None:
        raise SystemExit("Doukutsu.exe not found after extract")
    return exe.parent


def find_data_root(root: Path) -> Path:
    """Directory that contains data/Head.tsc (translation payload)."""
    hits = sorted(root.rglob("Head.tsc"))
    # Prefer …/data/Head.tsc
    for h in hits:
        if h.parent.name.lower() == "data":
            return h.parent.parent
    if hits:
        return hits[0].parent
    raise SystemExit("no data/Head.tsc in locale archive (not a Cave Story data pack?)")


def copy_engine_files(tree: Path) -> None:
    eng = ROOT / "third_party" / "nxengine"
    for name in ENGINE_FILES:
        src = eng / name
        if not src.is_file():
            raise SystemExit(f"missing engine support file: {src}")
        shutil.copy2(src, tree / name)
        print(f"  [engine] {name}")


def extract_from_doukutsu(tree: Path) -> None:
    exe = tree / "Doukutsu.exe"
    if not exe.is_file():
        raise SystemExit(f"missing {exe}")
    print(f"extracting from {exe}")
    blob = exe.read_bytes()
    extract_files(blob, tree)
    extract_pxt(blob, tree)
    extract_stages(blob, tree)


def prepare_english(outdir: Path, archive: Path | None, url: str, keep: bool) -> Path:
    tmp_dir = None
    try:
        if archive is not None:
            zip_path = archive.resolve()
            if not zip_path.is_file():
                raise SystemExit(f"archive not found: {zip_path}")
        else:
            if keep:
                zip_path = outdir / "cavestoryen.zip"
            else:
                tmp_dir = tempfile.TemporaryDirectory(prefix="cavestoryen-")
                zip_path = Path(tmp_dir.name) / "cavestoryen.zip"
            download(url, zip_path)

        extract_root = outdir / "_extract_en"
        if extract_root.exists():
            shutil.rmtree(extract_root)
        extract_archive(zip_path, extract_root)
        tree = find_doukutsu_tree(extract_root)

        canonical = outdir / "CaveStory"
        if canonical.exists():
            shutil.rmtree(canonical)
        shutil.move(str(tree), str(canonical))
        shutil.rmtree(extract_root, ignore_errors=True)

        extract_from_doukutsu(canonical)
        copy_engine_files(canonical)
        return canonical
    finally:
        if tmp_dir is not None:
            tmp_dir.cleanup()


def merge_overlay(tree: Path, overlay_root: Path) -> None:
    """Replace CaveStory/data/ with the translation payload; keep music/extract."""
    data_src = overlay_root / "data"
    if not data_src.is_dir():
        raise SystemExit(f"overlay missing data/: {overlay_root}")

    print(f"merging overlay data/ from {overlay_root}")
    data_dst = tree / "data"
    if data_dst.exists():
        shutil.rmtree(data_dst)

    def _ignore(_dir: str, names: list[str]) -> set[str]:
        skip = set()
        for n in names:
            low = n.lower()
            if low.endswith(".old") or low.endswith(".bak"):
                skip.add(n)
        return skip

    shutil.copytree(data_src, data_dst, ignore=_ignore)
    nfiles = sum(1 for p in data_dst.rglob("*") if p.is_file())
    print(f"  [overlay] data/ ({nfiles} files)")


def prepare_overlay_locale(
    outdir: Path,
    locale_archive: Path | None,
    locale_url: str,
    base_url: str,
    keep: bool,
) -> Path:
    # 1) English base with Doukutsu extract
    tree = prepare_english(outdir, archive=None, url=base_url, keep=keep)

    tmp_dir = None
    try:
        if locale_archive is not None:
            arch = locale_archive.resolve()
            if not arch.is_file():
                raise SystemExit(f"locale archive not found: {arch}")
        else:
            suffix = ".7z" if locale_url.lower().endswith(".7z") else ".zip"
            if keep:
                arch = outdir / f"cavestory_locale{suffix}"
            else:
                tmp_dir = tempfile.TemporaryDirectory(prefix="cavestory-locale-")
                arch = Path(tmp_dir.name) / f"locale{suffix}"
            download(locale_url, arch)

        extract_root = outdir / "_extract_locale"
        if extract_root.exists():
            shutil.rmtree(extract_root)
        extract_archive(arch, extract_root)
        overlay = find_data_root(extract_root)
        merge_overlay(tree, overlay)
        shutil.rmtree(extract_root, ignore_errors=True)
        return tree
    finally:
        if tmp_dir is not None:
            tmp_dir.cleanup()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--locale",
        default="en",
        help="locale id (default: en). Use --list-locales",
    )
    ap.add_argument(
        "--list-locales",
        action="store_true",
        help="print known locales and exit",
    )
    ap.add_argument(
        "--url",
        default=None,
        help="override download URL for the selected locale",
    )
    ap.add_argument(
        "--zip",
        "--archive",
        dest="archive",
        type=Path,
        default=None,
        help="use a local archive instead of downloading the locale pack",
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
        help="keep downloaded archives under outdir/",
    )
    args = ap.parse_args()

    if args.list_locales:
        list_locales()
        return 0

    loc = get_locale(args.locale)
    url = args.url or loc.url
    outdir = args.outdir.resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    print(f"locale={loc.id} ({loc.name}) kind={loc.kind}")
    print(f"nxpk target name: {loc.nxpk}")

    if loc.kind == "full":
        tree = prepare_english(outdir, args.archive, url, args.keep_zip)
    elif loc.kind == "overlay":
        base = get_locale("en")
        tree = prepare_overlay_locale(
            outdir,
            locale_archive=args.archive,
            locale_url=url,
            base_url=base.url,
            keep=args.keep_zip,
        )
    else:
        raise SystemExit(f"unsupported locale kind: {loc.kind}")

    print(f"Cave Story tree ready at {tree}")
    print("Next: ./CaveStory_host --ci-prepare && make pack-assets LOCALE=" + loc.id)
    return 0


if __name__ == "__main__":
    sys.exit(main())
