#!/usr/bin/env python3
"""Bake a used-glyph CJK atlas (cjkfont.dat) for ja/ko Cave Story packs.

Scans decrypted UTF-8 TSC (and optional extra strings), rasterizes glyphs with
Pillow + a CJK TTF/OTF, writes the CJK1 binary format consumed by
third_party/nxengine/graphics/cjkfont.cpp.

Format:
  magic u32 'CJK1' LE
  cell_w u8, cell_h u8, count u16
  count × { cp u32, offset u32, advance u8, pad[3] }  (sorted by cp)
  glyph bitmaps at absolute offsets: 1bpp MSB-first, row-major, cell_w×cell_h bits

Rasterization notes (legibility at ~12px on 320×240):
  - Prefer Medium weight (Bold fills counters; Light drops strokes)
  - Render at OVERSAMPLE× then box-downsample + threshold (keeps 漢/日 holes open)
  - Fullwidth advance = cell (monospace grid) for stable dialogue layout
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CJK_MAGIC = 0x314B4A43  # 'CJK1' LE
# Fits MSG_LINE_SPACING (16) with a little padding; 10px was too muddy for kanji.
DEFAULT_CELL = 12
OVERSAMPLE = 4

# Medium weight reads best at 12px after downsample. Bold/W6 fills counters;
# light W3/Regular drops thin strokes. Prefer Medium, then Regular, then Bold.
FONT_CANDIDATES_JA = (
    "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Medium.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Regular.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W5.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W6.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Bold.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
)

FONT_CANDIDATES_KO = (
    "/usr/share/fonts/opentype/noto/NotoSansCJKkr-Medium.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJKkr-Regular.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/System/Library/Fonts/AppleSDGothicNeo.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W5.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
)


def tsc_decrypt_bytes(data: bytes) -> bytes:
    if not data:
        return data
    buf = bytearray(data)
    keypos = len(buf) // 2
    dkey = buf[keypos]
    for i in range(keypos):
        buf[i] = (buf[i] - dkey) & 0xFF
    for i in range(keypos + 1, len(buf)):
        buf[i] = (buf[i] - dkey) & 0xFF
    return bytes(buf)


def decode_script_bytes(raw: bytes, encoding: str) -> str:
    """Decrypt (Cave Story mid-byte key), then decode with locale encoding → str."""
    dec = tsc_decrypt_bytes(raw)
    return dec.decode(encoding, errors="replace")


def collect_codepoints(root: Path, encoding: str) -> set[int]:
    cps: set[int] = set()
    for ch in " …—""''":
        cps.add(ord(ch))

    for path in sorted(root.rglob("*.tsc")):
        raw = path.read_bytes()
        text = decode_script_bytes(raw, encoding)
        for ch in text:
            o = ord(ch)
            if o >= 0x80:
                cps.add(o)

    for path in sorted(root.rglob("*.txt")):
        if "manual" in path.parts:
            continue
        try:
            text = path.read_bytes().decode(encoding, errors="ignore")
        except OSError:
            continue
        for ch in text:
            o = ord(ch)
            if o >= 0x80:
                cps.add(o)

    return cps


def find_font(explicit: Path | None, locale: str) -> Path:
    if explicit is not None:
        if not explicit.is_file():
            raise SystemExit(f"font not found: {explicit}")
        return explicit

    candidates = FONT_CANDIDATES_KO if locale == "ko" else FONT_CANDIDATES_JA
    for p in candidates:
        path = Path(p)
        if path.is_file():
            return path

    raise SystemExit(
        "no CJK font found; install fonts-noto-cjk (apt) or pass --font PATH"
    )


def _load_font(font_path: Path, px: int) -> ImageFont.FreeTypeFont:
    """Load TTF/OTF/TTC; try several face indices for Noto TTC collections."""
    last: OSError | None = None
    for index in (0, 1, 2, 3, 4):
        try:
            return ImageFont.truetype(str(font_path), size=px, index=index)
        except OSError as e:
            last = e
            continue
    assert last is not None
    raise last


def rasterize_glyph(
    font_path: Path, cp: int, cell: int, scale: int = OVERSAMPLE
) -> tuple[bytes, int]:
    """Return (1bpp packed bitmap cell×cell, advance_px)."""
    ch = chr(cp)
    big_cell = cell * scale
    # Slightly under cell so glyphs don't clip after downsample.
    font = _load_font(font_path, px=big_cell - scale)

    img = Image.new("L", (big_cell, big_cell), 0)
    draw = ImageDraw.Draw(img)
    try:
        bbox = draw.textbbox((0, 0), ch, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        # Center horizontally; slight upward bias vs Latin smalfont band.
        x = max(0, (big_cell - tw) // 2 - bbox[0])
        y = max(0, (big_cell - th) // 2 - bbox[1] - scale // 2)
        draw.text((x, y), ch, fill=255, font=font)
    except Exception:
        mask = font.getmask(ch, mode="L")
        mw, mh = mask.size
        glyph = Image.new("L", (mw, mh))
        glyph.putdata(list(mask))
        x = max(0, (big_cell - mw) // 2)
        y = max(0, (big_cell - mh) // 2)
        img.paste(glyph, (x, y))

    # Box/average downsample keeps open counters (holes) in 漢/日/etc.
    # Max-pool filled them solid. Threshold a bit above mid for clean edges.
    try:
        resample = Image.Resampling.BOX
    except AttributeError:
        resample = getattr(Image, "BOX", Image.BILINEAR)
    small = img.resize((cell, cell), resample)

    px = small.load()
    bits = bytearray((cell * cell + 7) // 8)
    for y in range(cell):
        for x in range(cell):
            # ~128 keeps counters open; much higher drops thin bars (口 bottom).
            if px[x, y] >= 128:
                bit = y * cell + x
                bits[bit >> 3] |= 0x80 >> (bit & 7)

    # Fullwidth grid: keep dialogue columns stable (classic JP doukutsu feel).
    adv = cell
    return bytes(bits), adv

def bake(codepoints: set[int], font_path: Path, cell: int, out: Path) -> None:
    ordered = sorted(codepoints)
    toc: list[tuple[int, int, int, bytes]] = []
    header_size = 8
    toc_size = len(ordered) * 12
    offset = (header_size + toc_size + 3) & ~3

    for cp in ordered:
        bits, adv = rasterize_glyph(font_path, cp, cell)
        toc.append((cp, offset, adv, bits))
        offset += len(bits)

    body = bytearray(offset)
    struct.pack_into("<IBBH", body, 0, CJK_MAGIC, cell, cell, len(ordered))
    for i, (cp, off, adv, bits) in enumerate(toc):
        base = header_size + i * 12
        struct.pack_into("<IIB3x", body, base, cp, off, adv & 0xFF)
        body[off : off + len(bits)] = bits

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bytes(body))
    print(
        f"wrote {out} — {len(ordered)} glyphs, cell={cell}px, "
        f"{len(body)} bytes ({len(body) / 1024:.1f} KiB) font={font_path.name} "
        f"oversample={OVERSAMPLE}x"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("root", nargs="?", default="CaveStory", type=Path)
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="default: <root>/cjkfont.dat",
    )
    ap.add_argument(
        "--encoding",
        default=None,
        help="source TSC encoding before UTF-8 rewrite (cp932/cp949)",
    )
    ap.add_argument("--locale", default="ja", help="ja|ko (font preference)")
    ap.add_argument("--font", type=Path, default=None)
    ap.add_argument("--cell", type=int, default=DEFAULT_CELL)
    ap.add_argument(
        "--codepoints-from-utf8",
        action="store_true",
        help="treat .tsc as already UTF-8 (post-conversion)",
    )
    args = ap.parse_args()

    enc = args.encoding
    if enc is None:
        enc = "utf-8" if args.codepoints_from_utf8 else (
            "cp949" if args.locale == "ko" else "cp932"
        )

    root = args.root
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 1

    cps = collect_codepoints(root, enc)
    if not cps:
        print("error: no non-ASCII codepoints found in TSC", file=sys.stderr)
        return 1
    print(f"collected {len(cps)} unique non-ASCII codepoints (encoding={enc})")

    font_path = find_font(args.font, args.locale)
    print(f"using font: {font_path}")
    out = args.output or (root / "cjkfont.dat")
    bake(cps, font_path, args.cell, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
