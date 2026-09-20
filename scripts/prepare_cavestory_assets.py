#!/usr/bin/env python3
"""
Pre-convert Cave Story 4bpp BMP/PBM sheets to 8bpp BMP siblings.

Runtime on G&W must NOT unpack+write derived blobs into flash. Instead:

  Foo.pbm  (4bpp original)  →  Foo.u8.bmp  (8bpp, top-down, XIP-friendly)

NXEngine / sdl_gw prefer the .u8.bmp when present.

Usage:
  python3 scripts/prepare_cavestory_assets.py CaveStory
  python3 scripts/prepare_cavestory_assets.py CaveStory --force
  make prepare-assets
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


def read_bmp(path: Path):
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    data_off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    topdown = h < 0
    h = abs(h)
    if w <= 0 or h <= 0:
        raise ValueError(f"bad size {w}x{h}: {path}")
    return data, data_off, w, h, bpp, topdown


def u8_sibling(path: Path) -> Path:
    """Foo.pbm / Foo.bmp → Foo.u8.bmp next to the original."""
    return path.with_name(path.stem + ".u8.bmp")


def convert_1_to_8(src: Path, dst: Path) -> tuple[int, int]:
    data, data_off, w, h, bpp, topdown = read_bmp(src)
    if bpp != 1:
        raise ValueError(f"expected 1bpp, got {bpp}: {src}")

    ncolors = 2
    pal_off = 54
    pal = bytearray(256 * 4)
    src_pal = data[pal_off : pal_off + ncolors * 4]
    pal[: len(src_pal)] = src_pal

    row_bytes_1 = ((w * 1 + 31) // 32) * 4
    row_bytes_8 = ((w * 8 + 31) // 32) * 4
    pixels = bytearray(row_bytes_8 * h)

    for y in range(h):
        src_y = y if topdown else (h - 1 - y)
        row = data[data_off + src_y * row_bytes_1 : data_off + (src_y + 1) * row_bytes_1]
        out = memoryview(pixels)[y * row_bytes_8 : (y + 1) * row_bytes_8]
        for x in range(w):
            byte = row[x // 8]
            bit = 7 - (x % 8)
            out[x] = 1 if (byte >> bit) & 1 else 0

    pixel_bytes = bytes(pixels)
    file_size = 14 + 40 + 256 * 4 + len(pixel_bytes)
    hdr = bytearray(14 + 40)
    hdr[0:2] = b"BM"
    struct.pack_into("<I", hdr, 2, file_size)
    struct.pack_into("<I", hdr, 10, 14 + 40 + 256 * 4)
    struct.pack_into("<I", hdr, 14, 40)
    struct.pack_into("<i", hdr, 18, w)
    struct.pack_into("<i", hdr, 22, -h)
    struct.pack_into("<H", hdr, 26, 1)
    struct.pack_into("<H", hdr, 28, 8)
    struct.pack_into("<I", hdr, 30, 0)
    struct.pack_into("<I", hdr, 34, len(pixel_bytes))
    struct.pack_into("<I", hdr, 46, 256)

    dst.write_bytes(bytes(hdr) + bytes(pal) + pixel_bytes)
    return w, h


def convert_4_to_8(src: Path, dst: Path) -> tuple[int, int]:
    data, data_off, w, h, bpp, topdown = read_bmp(src)
    if bpp != 4:
        raise ValueError(f"expected 4bpp, got {bpp}: {src}")

    ncolors = 16
    pal_off = 54
    pal = bytearray(256 * 4)
    src_pal = data[pal_off : pal_off + ncolors * 4]
    pal[: len(src_pal)] = src_pal

    row_bytes_4 = ((w * 4 + 31) // 32) * 4
    row_bytes_8 = ((w * 8 + 31) // 32) * 4
    pixels = bytearray(row_bytes_8 * h)

    for y in range(h):
        src_y = y if topdown else (h - 1 - y)
        row = data[data_off + src_y * row_bytes_4 : data_off + (src_y + 1) * row_bytes_4]
        out = memoryview(pixels)[y * row_bytes_8 : (y + 1) * row_bytes_8]
        for x in range(w):
            v = row[x // 2]
            out[x] = (v >> 4) if (x & 1) == 0 else (v & 0x0F)

    # Top-down 8bpp BMP (negative height) — matches our XIP loader preference.
    pixel_bytes = bytes(pixels)
    file_size = 14 + 40 + 256 * 4 + len(pixel_bytes)
    hdr = bytearray(14 + 40)
    hdr[0:2] = b"BM"
    struct.pack_into("<I", hdr, 2, file_size)
    struct.pack_into("<I", hdr, 10, 14 + 40 + 256 * 4)  # pixel offset
    struct.pack_into("<I", hdr, 14, 40)  # DIB size
    struct.pack_into("<i", hdr, 18, w)
    struct.pack_into("<i", hdr, 22, -h)  # top-down
    struct.pack_into("<H", hdr, 26, 1)  # planes
    struct.pack_into("<H", hdr, 28, 8)  # bpp
    struct.pack_into("<I", hdr, 30, 0)  # BI_RGB
    struct.pack_into("<I", hdr, 34, len(pixel_bytes))
    struct.pack_into("<I", hdr, 46, 256)  # colors used

    dst.write_bytes(bytes(hdr) + bytes(pal) + pixel_bytes)
    return w, h


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "root",
        nargs="?",
        default="CaveStory",
        help="Cave Story tree (default: CaveStory)",
    )
    ap.add_argument(
        "--force",
        action="store_true",
        help="overwrite existing .u8.bmp files",
    )
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="list work without writing",
    )
    args = ap.parse_args()
    root = Path(args.root)
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 1

    converted = skipped = already = errors = 0
    for path in sorted(root.rglob("*")):
        if path.suffix.lower() not in (".pbm", ".bmp"):
            continue
        if path.name.endswith(".u8.bmp"):
            continue
        try:
            _, _, w, h, bpp, _ = read_bmp(path)
        except ValueError as e:
            print(f"skip {path}: {e}")
            errors += 1
            continue
        if bpp != 4 and bpp != 1:
            skipped += 1
            continue
        out = u8_sibling(path)
        if out.exists() and not args.force:
            already += 1
            continue
        rel = path.relative_to(root)
        if args.dry_run:
            print(f"would convert {rel} ({w}x{h} {bpp}bpp) -> {out.name}")
            converted += 1
            continue
        try:
            if bpp == 1:
                convert_1_to_8(path, out)
            else:
                convert_4_to_8(path, out)
            print(f"ok {rel} ({w}x{h} {bpp}bpp) -> {out.relative_to(root)}")
            converted += 1
        except Exception as e:
            print(f"error {rel}: {e}", file=sys.stderr)
            errors += 1

    print(
        f"done: converted={converted} already={already} "
        f"skipped_non4={skipped} errors={errors}"
    )
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
