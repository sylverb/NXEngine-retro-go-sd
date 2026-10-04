#!/usr/bin/env python3
"""Rebuild third_party/nxengine/smalfont.bmp with CP1252 Latin accents.

Starts from the ASCII-only sheet (254×33) and appends Western-European
glyphs used by FR/DE/ES/IT/NL/FI/PT TSC packs. Run from repo root:

  python3 scripts/extend_smalfont_latin.py
  # optional: --base /path/to/ascii_smalfont.bmp
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASE = ROOT / "third_party" / "nxengine" / "smalfont.bmp"
DEFAULT_OUT = DEFAULT_BASE

CW, CH = 5, 9
SW, SH = 8, 12
FG, BG = 19, 0

# Must match font.cpp ASCII portion of bitmap_map.
BITMAP_ASCII = (
    ' !"#$%&`()*+,-./0123456789:;<=>?'
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]%_"
    "'abcdefghijklmnopqrstuvwxyz{|}~"
)

ACUTE = [(3, 0), (2, 1)]
GRAVE = [(1, 0), (2, 1)]
CIRC = [(2, 0), (1, 1), (3, 1)]
DIER = [(1, 0), (3, 1)]  # slightly lower second dot for 5×9
TILDE = [(1, 0), (2, 0), (3, 0), (2, 1)]
CEDILLA = [(2, 7), (1, 8), (2, 8), (3, 8)]
RING = [(2, 0)]  # Å-ish (unused for now)


def cell_xy(index: int, sheet_w: int) -> tuple[int, int]:
    x = y = 0
    for _ in range(index):
        x += SW
        if x >= sheet_w:
            x = 0
            y += SH
    return x, y


def get_glyph(px, ch: str, sheet_w: int) -> list[list[int]]:
    i = BITMAP_ASCII.index(ch)
    x0, y0 = cell_xy(i, sheet_w)
    return [[1 if px[x0 + x, y0 + y] == FG else 0 for x in range(CW)] for y in range(CH)]


def shift_down(g: list[list[int]], n: int = 1) -> list[list[int]]:
    out = [[0] * CW for _ in range(CH)]
    for y in range(CH - n):
        out[y + n] = g[y][:]
    return out


def or_pixels(g: list[list[int]], coords) -> list[list[int]]:
    g = [row[:] for row in g]
    for x, y in coords:
        if 0 <= x < CW and 0 <= y < CH:
            g[y][x] = 1
    return g


def accent_lower(px, base: str, accent, sheet_w: int) -> list[list[int]]:
    orig = get_glyph(px, base, sheet_w)
    g = [[0] * CW for _ in range(CH)]
    for y in range(2, CH):
        g[y] = orig[y][:]
    return or_pixels(g, accent)


def accent_upper(px, base: str, accent, sheet_w: int) -> list[list[int]]:
    return or_pixels(shift_down(get_glyph(px, base, sheet_w), 1), accent)


def cedilla(px, base: str, sheet_w: int) -> list[list[int]]:
    return or_pixels(get_glyph(px, base, sheet_w), CEDILLA)


def make_ss(px, sheet_w: int) -> list[list[int]]:
    """Compact ß from stacked s shapes."""
    s = get_glyph(px, "s", sheet_w)
    g = [[0] * CW for _ in range(CH)]
    # top bowl
    for y in range(2, 6):
        g[y] = s[y][:]
    # bottom bowl shifted
    for y in range(4, 8):
        for x in range(CW):
            if s[y - 2][x]:
                g[y][x] = 1
    g[0] = [0, 1, 1, 1, 0]
    g[1] = [1, 0, 0, 0, 0]
    return g


def make_oe() -> list[list[int]]:
    pattern = [
        ".....",
        ".....",
        "##.##",
        "#.#.#",
        "#####",
        "#.#..",
        "##.##",
        ".....",
        ".....",
    ]
    return [[1 if c == "#" else 0 for c in row] for row in pattern]


def make_inverted(px, ch: str, sheet_w: int) -> list[list[int]]:
    g = get_glyph(px, ch, sheet_w)
    return list(reversed(g))


def make_ord_masc() -> list[list[int]]:
    # º
    pattern = [
        ".###.",
        "#...#",
        "#...#",
        ".###.",
        "..#..",
        ".....",
        ".....",
        ".....",
        ".....",
    ]
    return [[1 if c == "#" else 0 for c in row] for row in pattern]


def make_quote(kind: str) -> list[list[int]]:
    g = [[0] * CW for _ in range(CH)]
    if kind == "ldquo":  # “
        g[0][1] = g[0][3] = 1
        g[1][1] = g[1][3] = 1
        g[2][0] = g[2][2] = 1
    elif kind == "rdquo":  # ”
        g[0][0] = g[0][2] = 1
        g[1][1] = g[1][3] = 1
        g[2][1] = g[2][3] = 1
    elif kind == "ellipsis":  # …
        g[6][0] = g[6][2] = g[6][4] = 1
    elif kind == "rsquo":  # ’
        g[0][2] = 1
        g[1][2] = 1
        g[2][1] = 1
    return g


def build_extras(px, sheet_w: int) -> list[tuple[int, list[list[int]]]]:
    """Ordered CP1252 bytes + glyphs (must match font.cpp bitmap_map)."""
    G = []

    def add(b: int, glyph):
        G.append((b, glyph))

    # French / shared
    add(0xC0, accent_upper(px, "A", GRAVE, sheet_w))
    add(0xC1, accent_upper(px, "A", ACUTE, sheet_w))
    add(0xC2, accent_upper(px, "A", CIRC, sheet_w))
    add(0xC3, accent_upper(px, "A", TILDE, sheet_w))
    add(0xC4, accent_upper(px, "A", DIER, sheet_w))
    add(0xC7, cedilla(px, "C", sheet_w))
    add(0xC8, accent_upper(px, "E", GRAVE, sheet_w))
    add(0xC9, accent_upper(px, "E", ACUTE, sheet_w))
    add(0xCA, accent_upper(px, "E", CIRC, sheet_w))
    add(0xCD, accent_upper(px, "I", ACUTE, sheet_w))
    add(0xCE, accent_upper(px, "I", CIRC, sheet_w))
    add(0xD1, accent_upper(px, "N", TILDE, sheet_w))
    add(0xD3, accent_upper(px, "O", ACUTE, sheet_w))
    add(0xD4, accent_upper(px, "O", CIRC, sheet_w))
    add(0xD6, accent_upper(px, "O", DIER, sheet_w))
    add(0xDA, accent_upper(px, "U", ACUTE, sheet_w))
    add(0xDC, accent_upper(px, "U", DIER, sheet_w))
    add(0xDF, make_ss(px, sheet_w))
    add(0xE0, accent_lower(px, "a", GRAVE, sheet_w))
    add(0xE1, accent_lower(px, "a", ACUTE, sheet_w))
    add(0xE2, accent_lower(px, "a", CIRC, sheet_w))
    add(0xE3, accent_lower(px, "a", TILDE, sheet_w))
    add(0xE4, accent_lower(px, "a", DIER, sheet_w))
    add(0xE7, cedilla(px, "c", sheet_w))
    add(0xE8, accent_lower(px, "e", GRAVE, sheet_w))
    add(0xE9, accent_lower(px, "e", ACUTE, sheet_w))
    add(0xEA, accent_lower(px, "e", CIRC, sheet_w))
    add(0xEB, accent_lower(px, "e", DIER, sheet_w))
    add(0xEC, accent_lower(px, "i", GRAVE, sheet_w))
    add(0xED, accent_lower(px, "i", ACUTE, sheet_w))
    add(0xEE, accent_lower(px, "i", CIRC, sheet_w))
    add(0xEF, accent_lower(px, "i", DIER, sheet_w))
    add(0xF1, accent_lower(px, "n", TILDE, sheet_w))
    add(0xF2, accent_lower(px, "o", GRAVE, sheet_w))
    add(0xF3, accent_lower(px, "o", ACUTE, sheet_w))
    add(0xF4, accent_lower(px, "o", CIRC, sheet_w))
    add(0xF5, accent_lower(px, "o", TILDE, sheet_w))
    add(0xF6, accent_lower(px, "o", DIER, sheet_w))
    add(0xF9, accent_lower(px, "u", GRAVE, sheet_w))
    add(0xFA, accent_lower(px, "u", ACUTE, sheet_w))
    add(0xFB, accent_lower(px, "u", CIRC, sheet_w))
    add(0xFC, accent_lower(px, "u", DIER, sheet_w))
    add(0x9C, make_oe())
    add(0xA1, make_inverted(px, "!", sheet_w))
    add(0xBF, make_inverted(px, "?", sheet_w))
    add(0xBA, make_ord_masc())
    add(0x92, make_quote("rsquo"))
    add(0x93, make_quote("ldquo"))
    add(0x94, make_quote("rdquo"))
    add(0x85, make_quote("ellipsis"))
    return G


def c_escape(extras: list[tuple[int, list[list[int]]]]) -> str:
    return "".join(f"\\x{b:02X}" for b, _ in extras)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", type=Path, default=None, help="ASCII-only smalfont.bmp")
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--print-c", action="store_true", help="print C string for font.cpp")
    args = ap.parse_args()

    base_path = args.base
    if base_path is None:
        # Prefer committed ASCII snapshot if regenerating from dirty tree.
        ascii_snap = Path("/tmp/smalfont_ascii.bmp")
        if ascii_snap.is_file() and Image.open(ascii_snap).size[1] <= 33:
            base_path = ascii_snap
        else:
            base_path = DEFAULT_BASE

    im = Image.open(base_path)
    if im.mode != "P":
        raise SystemExit(f"expected paletted BMP, got {im.mode}")
    w, h = im.size
    if h > 33:
        # Trim to ASCII rows if feeding an already-extended sheet.
        im = im.crop((0, 0, w, 33))
        h = 33
    px = im.load()

    extras = build_extras(px, w)
    n_total = len(BITMAP_ASCII) + len(extras)
    rows_needed = 0
    x = y = 0
    for _ in range(n_total):
        rows_needed = max(rows_needed, y + CH)
        x += SW
        if x >= w:
            x = 0
            y += SH
    new_h = ((rows_needed + SH - 1) // SH) * SH

    out = Image.new("P", (w, new_h), BG)
    pal = im.getpalette() or []
    while len(pal) < 256 * 3:
        pal.append(0)
    out.putpalette(pal)
    opx = out.load()
    for yy in range(h):
        for xx in range(w):
            opx[xx, yy] = px[xx, yy]

    x = y = 0
    for _ in range(len(BITMAP_ASCII)):
        x += SW
        if x >= w:
            x = 0
            y += SH

    for b, glyph in extras:
        for gy in range(CH):
            for gx in range(CW):
                if glyph[gy][gx]:
                    opx[x + gx, y + gy] = FG
        x += SW
        if x >= w:
            x = 0
            y += SH

    args.output.parent.mkdir(parents=True, exist_ok=True)
    out.save(args.output, format="BMP")
    print(f"wrote {args.output} size={out.size} extras={len(extras)}")
    esc = c_escape(extras)
    if args.print_c:
        # wrap ~16 bytes per line
        parts = [esc[i : i + 4 * 16] for i in range(0, len(esc), 4 * 16)]
        for p in parts:
            print(f'\t"{p}"')
    else:
        print("C fragment:", f'"{esc}"')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
