#!/usr/bin/env python3
"""Bake a used-glyph CJK atlas (cjkfont.dat) for ja/ko Cave Story packs.

Scans UTF-8 (or legacy) TSC, rasterizes glyphs with Pillow, writes CJK1 for
third_party/nxengine/graphics/cjkfont.cpp.

Default font: TakWolf Fusion Pixel (12px monospaced, ja/ko ms.bitmap.ttf).
True pixel strikes — render 1:1 at design size (no oversample). Noto CJK
downsample was muddy at dialogue size; Fusion Pixel is designed for this.

Format:
  magic u32 'CJK1' LE
  cell_w u8, cell_h u8, count u16
  count × { cp u32, offset u32, advance u8, pad[3] }  (sorted by cp)
  glyph bitmaps: 1bpp MSB-first, row-major, cell_w×cell_h bits
"""
from __future__ import annotations

import argparse
import struct
import sys
import urllib.request
import zipfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CJK_MAGIC = 0x314B4A43  # 'CJK1' LE
# Fits MSG_LINE_SPACING (16). Fusion Pixel ships exact 10px / 12px strikes.
DEFAULT_CELL = 12
FUSION_RELEASE = "2026.09.25"
FUSION_BASE = (
    f"https://github.com/TakWolf/fusion-pixel-font/releases/download/{FUSION_RELEASE}"
)

SCRIPT_DIR = Path(__file__).resolve().parent
CACHE_DIR = SCRIPT_DIR / ".cache" / "fusion-pixel"


def fusion_zip_url(cell: int) -> str:
    return (
        f"{FUSION_BASE}/fusion-pixel-font-{cell}px-monospaced-ms.bitmap.ttf-"
        f"v{FUSION_RELEASE}.zip"
    )


def fusion_ttf_name(cell: int, locale: str) -> str:
    loc = "ko" if locale == "ko" else "ja"
    return f"fusion-pixel-{cell}px-monospaced-{loc}.ms.bitmap.ttf"


def ensure_fusion_pixel(cell: int, locale: str) -> Path:
    """Download Fusion Pixel zip into scripts/.cache if needed; return TTF path."""
    if cell not in (10, 12):
        raise SystemExit(
            f"Fusion Pixel auto-fetch only supports cell 10 or 12 (got {cell}); "
            "pass --font for other sizes"
        )
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    ttf_name = fusion_ttf_name(cell, locale)
    ttf_path = CACHE_DIR / ttf_name
    if ttf_path.is_file():
        return ttf_path

    zip_name = (
        f"fusion-pixel-font-{cell}px-monospaced-ms.bitmap.ttf-v{FUSION_RELEASE}.zip"
    )
    zip_path = CACHE_DIR / zip_name
    url = fusion_zip_url(cell)
    if not zip_path.is_file():
        print(f"[CJK] downloading Fusion Pixel {cell}px → {zip_path.name}")
        print(f"      {url}")
        try:
            urllib.request.urlretrieve(url, zip_path)
        except Exception as exc:
            if zip_path.is_file():
                zip_path.unlink()
            raise SystemExit(f"failed to download Fusion Pixel: {exc}") from exc

    print(f"[CJK] extracting {ttf_name} from {zip_path.name}")
    with zipfile.ZipFile(zip_path, "r") as zf:
        try:
            data = zf.read(ttf_name)
        except KeyError as exc:
            names = [n for n in zf.namelist() if n.endswith(".ttf")]
            raise SystemExit(
                f"{ttf_name} missing in zip; available: {names}"
            ) from exc
    ttf_path.write_bytes(data)
    # Optional: keep OFL alongside cache for redistributors who inspect the dir.
    try:
        with zipfile.ZipFile(zip_path, "r") as zf:
            if "OFL.txt" in zf.namelist():
                (CACHE_DIR / "OFL.txt").write_bytes(zf.read("OFL.txt"))
    except OSError:
        pass
    return ttf_path


# Fallback outline fonts (muddy at 12px — last resort only).
FONT_FALLBACK_JA = (
    "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Medium.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W5.ttc",
)
FONT_FALLBACK_KO = (
    "/usr/share/fonts/opentype/noto/NotoSansCJKkr-Medium.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc",
    "/System/Library/Fonts/AppleSDGothicNeo.ttc",
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
    dec = tsc_decrypt_bytes(raw)
    return dec.decode(encoding, errors="replace")


def collect_stage_dat_codepoints(path: Path, encoding: str) -> set[int]:
    """Collect non-ASCII codepoints from stage.dat map captions."""
    cps: set[int] = set()
    try:
        data = path.read_bytes()
    except OSError:
        return cps
    if not data:
        return cps
    rec, name_off, name_len = 73, 32, 35
    n = data[0]
    for i in range(n):
        off = 1 + i * rec
        if off + rec > len(data):
            break
        raw = data[off + name_off : off + name_off + name_len].split(b"\0")[0]
        if not raw:
            continue
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            text = raw.decode(encoding, errors="replace")
        for ch in text:
            o = ord(ch)
            if o >= 0x80:
                cps.add(o)
    return cps


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

    # Map-name banner (center screen) uses stage.dat captions.
    for name in ("stage.dat", "Stage.dat"):
        p = root / name
        if p.is_file():
            cps |= collect_stage_dat_codepoints(p, encoding)
            break

    return cps


def is_pixel_font(font_path: Path) -> bool:
    n = font_path.name.lower()
    return "fusion-pixel" in n or "ms.bitmap" in n or n.endswith(".bdf")


def find_font(
    explicit: Path | None, locale: str, cell: int, *, allow_fetch: bool = True
) -> Path:
    if explicit is not None:
        if not explicit.is_file():
            raise SystemExit(f"font not found: {explicit}")
        return explicit

    cached = CACHE_DIR / fusion_ttf_name(cell, locale)
    if cached.is_file():
        return cached

    if allow_fetch:
        try:
            return ensure_fusion_pixel(cell, locale)
        except SystemExit as exc:
            print(
                f"[CJK] Fusion Pixel unavailable ({exc}); trying outline fallbacks",
                file=sys.stderr,
            )

    candidates = FONT_FALLBACK_KO if locale == "ko" else FONT_FALLBACK_JA
    for p in candidates:
        path = Path(p)
        if path.is_file():
            return path

    raise SystemExit(
        "no CJK font found. Need network to fetch Fusion Pixel, or pass --font PATH.\n"
        f"  expected cache: {CACHE_DIR / fusion_ttf_name(cell, locale)}"
    )


def _load_font(font_path: Path, px: int) -> ImageFont.FreeTypeFont:
    last: OSError | None = None
    for index in (0, 1, 2, 3, 4):
        try:
            return ImageFont.truetype(str(font_path), size=px, index=index)
        except OSError as e:
            last = e
            continue
    assert last is not None
    raise last


def rasterize_glyph_pixel(font_path: Path, cp: int, cell: int) -> tuple[bytes, int]:
    """1:1 pixel strike (Fusion Pixel). Must use font size == design size."""
    ch = chr(cp)
    font = _load_font(font_path, px=cell)
    img = Image.new("L", (cell, cell), 0)
    draw = ImageDraw.Draw(img)
    try:
        bbox = draw.textbbox((0, 0), ch, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        if tw <= 0 or th <= 0:
            # Missing glyph / wrong size — leave blank.
            return bytes((cell * cell + 7) // 8), cell
        x = max(0, (cell - tw) // 2 - bbox[0])
        y = max(0, (cell - th) // 2 - bbox[1])
        draw.text((x, y), ch, fill=255, font=font)
    except Exception:
        return bytes((cell * cell + 7) // 8), cell

    px = img.load()
    bits = bytearray((cell * cell + 7) // 8)
    for y in range(cell):
        for x in range(cell):
            if px[x, y] >= 128:
                bit = y * cell + x
                bits[bit >> 3] |= 0x80 >> (bit & 7)
    return bytes(bits), cell


def rasterize_glyph_outline(
    font_path: Path, cp: int, cell: int, scale: int = 4
) -> tuple[bytes, int]:
    """Outline font (Noto…): oversample then box-downsample (legacy path)."""
    ch = chr(cp)
    big_cell = cell * scale
    font = _load_font(font_path, px=big_cell - scale)

    img = Image.new("L", (big_cell, big_cell), 0)
    draw = ImageDraw.Draw(img)
    try:
        bbox = draw.textbbox((0, 0), ch, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
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

    try:
        resample = Image.Resampling.BOX
    except AttributeError:
        resample = getattr(Image, "BOX", Image.BILINEAR)
    small = img.resize((cell, cell), resample)

    px = small.load()
    bits = bytearray((cell * cell + 7) // 8)
    for y in range(cell):
        for x in range(cell):
            if px[x, y] >= 128:
                bit = y * cell + x
                bits[bit >> 3] |= 0x80 >> (bit & 7)
    return bytes(bits), cell


def bake(codepoints: set[int], font_path: Path, cell: int, out: Path) -> None:
    ordered = sorted(codepoints)
    pixel = is_pixel_font(font_path)
    toc: list[tuple[int, int, int, bytes]] = []
    header_size = 8
    toc_size = len(ordered) * 12
    offset = (header_size + toc_size + 3) & ~3

    for i, cp in enumerate(ordered):
        if pixel:
            bits, adv = rasterize_glyph_pixel(font_path, cp, cell)
        else:
            bits, adv = rasterize_glyph_outline(font_path, cp, cell)
        toc.append((cp, offset, adv, bits))
        offset += len(bits)
        if (i + 1) % 200 == 0:
            print(f"  … {i + 1}/{len(ordered)} glyphs")

    body = bytearray(offset)
    struct.pack_into("<IBBH", body, 0, CJK_MAGIC, cell, cell, len(ordered))
    for i, (cp, off, adv, bits) in enumerate(toc):
        base = header_size + i * 12
        struct.pack_into("<IIB3x", body, base, cp, off, adv & 0xFF)
        body[off : off + len(bits)] = bits

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bytes(body))
    mode = "pixel-1:1" if pixel else "outline-oversample"
    print(
        f"wrote {out} — {len(ordered)} glyphs, cell={cell}px, "
        f"{len(body)} bytes ({len(body) / 1024:.1f} KiB) font={font_path.name} "
        f"mode={mode}"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("root", nargs="?", default="CaveStory", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=None)
    ap.add_argument(
        "--encoding",
        default=None,
        help="source TSC encoding before UTF-8 rewrite (cp932/cp949)",
    )
    ap.add_argument("--locale", default="ja", help="ja|ko (Fusion Pixel face)")
    ap.add_argument("--font", type=Path, default=None, help="override TTF/OTF path")
    ap.add_argument(
        "--cell",
        type=int,
        default=DEFAULT_CELL,
        help="glyph cell size (10 or 12 for Fusion Pixel; default 12)",
    )
    ap.add_argument(
        "--codepoints-from-utf8",
        action="store_true",
        help="treat .tsc as already UTF-8 (post-conversion)",
    )
    ap.add_argument(
        "--no-fetch",
        action="store_true",
        help="do not download Fusion Pixel; only use --font or local cache/fallbacks",
    )
    args = ap.parse_args()

    enc = args.encoding
    if enc is None:
        enc = (
            "utf-8"
            if args.codepoints_from_utf8
            else ("cp949" if args.locale == "ko" else "cp932")
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

    font_path = find_font(
        args.font, args.locale, args.cell, allow_fetch=not args.no_fetch
    )
    print(f"using font: {font_path}")
    out = args.output or (root / "cjkfont.dat")
    bake(cps, font_path, args.cell, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
