#!/usr/bin/env python3
"""
Pack Cave Story assets into a single cavestory.nxpk for G&W XIP.

Format (version 1):
  magic "NXPK" (4) + version u16=1 + count u16
  count × { path[96] NUL-padded, offset u32, size u32 }
  blobs aligned to 4 bytes

Images are stored as 8bpp BMP under the *logical* game path (e.g. data/Npc/NpcSym.pbm).
TSC scripts are stored already decrypted (same algorithm as NXEngine tsc_decrypt).

Usage:
  python3 scripts/pack_cavestory_nxpk.py CaveStory
  python3 scripts/pack_cavestory_nxpk.py CaveStory -o CaveStory/cavestory.nxpk
  make pack-assets
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

NXPK_MAGIC = b"NXPK"
NXPK_VERSION = 1
PATH_LEN = 96

# Convert / include logic reused from prepare_cavestory_assets.py
sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare_cavestory_assets import (  # noqa: E402
    convert_1_to_8,
    convert_4_to_8,
    read_bmp,
    u8_sibling,
)

EXCLUDE_NAMES = {
    "doukutsu.exe",
    "doconfig.exe",
    "orgview.exe",
    "readme.txt",
    "manual.html",
    "config.dat",
    "settings.dat",
    "debug.txt",
    "thumbs.db",
    "cavestory.nxpk",
}
EXCLUDE_DIR_NAMES = {"manual", ".git"}
EXCLUDE_SUFFIXES = {".exe"}


def should_skip(rel: Path) -> bool:
    parts_lower = [p.lower() for p in rel.parts]
    if any(p in EXCLUDE_DIR_NAMES for p in parts_lower[:-1]):
        return True
    name = rel.name.lower()
    if name in EXCLUDE_NAMES:
        return True
    if rel.suffix.lower() in EXCLUDE_SUFFIXES:
        return True
    if name.endswith(".u8.bmp"):
        # Packed under the logical .pbm/.bmp name instead.
        return True
    # Profile / save slots stay on FatFs.
    if name.startswith("profile") and name.endswith(".dat"):
        return True
    return False


def tsc_decrypt_bytes(data: bytes) -> bytes:
    """Match third_party/nxengine/tsc.cpp tsc_decrypt (in-place subtract)."""
    if not data:
        return data
    buf = bytearray(data)
    fsize = len(buf)
    keypos = fsize // 2
    dkey = buf[keypos]
    for i in range(keypos):
        buf[i] = (buf[i] - dkey) & 0xFF
    for i in range(keypos + 1, fsize):
        buf[i] = (buf[i] - dkey) & 0xFF
    return bytes(buf)


def normalize_rel(path: Path) -> str:
    return path.as_posix()


def _bmp_to_u8_bytes(path: Path, convert_fn) -> bytes:
    import tempfile

    with tempfile.NamedTemporaryFile(suffix=".u8.bmp", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    try:
        convert_fn(path, tmp_path)
        return tmp_path.read_bytes()
    finally:
        tmp_path.unlink(missing_ok=True)


def image_blob_for(path: Path) -> bytes | None:
    """Return 8bpp BMP bytes for a .pbm/.bmp, or None to fall through to raw copy."""
    suffix = path.suffix.lower()
    if suffix not in (".pbm", ".bmp"):
        return None
    try:
        _, _, _, _, bpp, _ = read_bmp(path)
    except ValueError:
        return None

    if bpp == 8:
        return path.read_bytes()

    if bpp in (1, 4):
        sibling = u8_sibling(path)
        if sibling.is_file():
            return sibling.read_bytes()
        convert_fn = convert_1_to_8 if bpp == 1 else convert_4_to_8
        return _bmp_to_u8_bytes(path, convert_fn)

    # 16/24 etc. — pack as-is
    return path.read_bytes()


def collect_entries(root: Path) -> list[tuple[str, bytes]]:
    entries: list[tuple[str, bytes]] = []
    seen: set[str] = set()

    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root)
        if should_skip(rel):
            continue
        key = normalize_rel(rel)
        if key in seen:
            continue

        data: bytes
        if path.suffix.lower() == ".tsc":
            data = tsc_decrypt_bytes(path.read_bytes())
            kind = "tsc-clear"
        else:
            img = image_blob_for(path)
            if img is not None:
                data = img
                kind = "img8"
            else:
                data = path.read_bytes()
                kind = "raw"

        if len(key) >= PATH_LEN:
            raise SystemExit(f"path too long ({len(key)} >= {PATH_LEN}): {key}")

        entries.append((key, data))
        seen.add(key)
        print(f"  [{kind:9}] {key} ({len(data)} bytes)")

    return entries


def write_nxpk(entries: list[tuple[str, bytes]], out: Path) -> None:
    count = len(entries)
    header_size = 8 + count * (PATH_LEN + 8)
    blob_base = (header_size + 3) & ~3

    # First pass: compute absolute offsets
    offsets: list[int] = []
    cur = blob_base
    for _, data in entries:
        cur = (cur + 3) & ~3
        offsets.append(cur)
        cur += len(data)

    toc = bytearray()
    for (path, data), off in zip(entries, offsets):
        path_bytes = path.encode("ascii", errors="replace")[: PATH_LEN - 1]
        path_field = path_bytes + b"\0" * (PATH_LEN - len(path_bytes))
        toc += path_field
        toc += struct.pack("<II", off, len(data))

    body = bytearray(cur)
    body[:8] = NXPK_MAGIC + struct.pack("<HH", NXPK_VERSION, count)
    body[8:header_size] = toc
    for (path, data), off in zip(entries, offsets):
        body[off : off + len(data)] = data

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bytes(body))
    total = len(body)
    print(f"wrote {out} — {count} entries, {total} bytes ({total / (1024 * 1024):.2f} MiB)")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("root", nargs="?", default="CaveStory", help="Cave Story tree")
    ap.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output path (default: <root>/cavestory.nxpk)",
    )
    ap.add_argument(
        "--also-sd",
        action="store_true",
        help="Also copy to sd_content/homebrews/cavestory.nxpk",
    )
    args = ap.parse_args()
    root = Path(args.root)
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 1

    out = Path(args.output) if args.output else root / "cavestory.nxpk"
    print(f"packing {root} → {out}")
    entries = collect_entries(root)
    if not entries:
        print("error: no files packed", file=sys.stderr)
        return 1
    write_nxpk(entries, out)

    if args.also_sd:
        sd = Path("sd_content/homebrews/cavestory.nxpk")
        sd.parent.mkdir(parents=True, exist_ok=True)
        sd.write_bytes(out.read_bytes())
        print(f"copied → {sd}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
