#!/usr/bin/env python3
"""Patch Cave Story UI bitmaps (Yes/No, AIR, title menu) for CJK locales.

These strings live in TextBox.pbm / Title.pbm sprites, not TSC — so a scripts-only
Korean overlay still shows Japanese はい/いいえ, くうき, 洞窟物語, etc.

Usage:
  python3 scripts/localize_ui_sprites.py CaveStory --locale ko
"""
from __future__ import annotations

import argparse
import io
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bake_cjk_atlas import ensure_fusion_pixel  # noqa: E402

YESNO = dict(sheet="TextBox.pbm", x=152, y=48, w=84, h=32)
AIR_X, AIR_W, AIR_H = 112, 32, 8
AIR_YS = (72, 80)
# ESC pause prompt (SPR_RESETPROMPT) — hand-drawn in stock TextBox.pbm
RESETPROMPT = dict(x=0, y=128, w=208, h=16)
TITLE = dict(x=0, y=0, w=140, h=32)
MENU = dict(x=140, w=40, h=16)

COL_WHITE = (255, 255, 255, 255)
COL_SHADOW = (25, 33, 66, 255)
COL_TITLE = (247, 247, 234, 255)
COL_YESNO_BG = (234, 25, 62, 255)

# Wipe these when clearing Yes/No text (keep red/yellow chrome).
YESNO_TEXT_COLORS = {
    (255, 255, 255),
    (25, 33, 66),
    (29, 62, 99),
    (12, 23, 33),
}

STRINGS_KO = {
    "yesno": "예 / 아니오",
    "air": "공기",
    # Keep the big English "Cave Story" wordmark (same as fr/de/… overlays).
    # Only the New/Load menu cells are rewritten — 40×16 → short labels.
    "menu0": "시작",
    "menu1": "계속",
}

REPO_ROOT = Path(__file__).resolve().parent.parent


def open_sheet(path: Path) -> tuple[Image.Image, Image.Image | None]:
    src = Image.open(path)
    if src.mode == "P":
        return src.convert("RGBA"), src
    return src.convert("RGBA"), None


def save_sheet(path: Path, rgba: Image.Image, pal_src: Image.Image | None) -> None:
    # Cave Story "`.pbm`" files are Windows BMPs (magic BM), not netpbm.
    if pal_src is not None and pal_src.mode == "P":
        rgba.convert("RGB").quantize(palette=pal_src).save(path, format="BMP")
    else:
        rgba.convert("RGB").save(path, format="BMP")


def render_text(
    text: str,
    font: ImageFont.FreeTypeFont,
    *,
    fill,
    shadow=None,
) -> Image.Image:
    dummy = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    bbox = dummy.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    pad = 1
    img = Image.new("RGBA", (tw + pad * 2 + 2, th + pad * 2 + 2), (0, 0, 0, 0))
    dr = ImageDraw.Draw(img)
    origin = (pad - bbox[0], pad - bbox[1])
    if shadow is not None:
        dr.text((origin[0] + 1, origin[1] + 1), text, font=font, fill=shadow)
    dr.text(origin, text, font=font, fill=fill)
    trimmed = img.getbbox()
    return img.crop(trimmed) if trimmed else img


def fit(glyph: Image.Image, max_w: int, max_h: int) -> Image.Image:
    """Shrink only if larger than the box (never upscale)."""
    gw, gh = glyph.size
    if gw <= max_w and gh <= max_h:
        return glyph
    scale = min(max_w / gw, max_h / gh)
    return glyph.resize(
        (max(1, int(gw * scale)), max(1, int(gh * scale))),
        Image.Resampling.NEAREST,
    )


def paste_black_centered(sheet: Image.Image, glyph: Image.Image, box: dict) -> None:
    x, y, w, h = box["x"], box["y"], box["w"], box["h"]
    region = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    gw, gh = glyph.size
    region.alpha_composite(glyph, (max(0, (w - gw) // 2), max(0, (h - gh) // 2)))
    sheet.paste(region, (x, y))


def paste_yesno(sheet: Image.Image, glyph: Image.Image) -> None:
    x, y, w, h = YESNO["x"], YESNO["y"], YESNO["w"], YESNO["h"]
    region = sheet.crop((x, y, x + w, y + h)).convert("RGBA")
    px = region.load()
    for yy in range(h):
        for xx in range(w):
            r, g, b, _a = px[xx, yy]
            if (r, g, b) in YESNO_TEXT_COLORS:
                px[xx, yy] = COL_YESNO_BG
    glyph = fit(glyph, w - 8, h - 8)
    gw, gh = glyph.size
    region.alpha_composite(glyph, (max(0, (w - gw) // 2), max(0, (h - gh) // 2)))
    sheet.paste(region, (x, y))


def paste_air(sheet: Image.Image, glyph: Image.Image, y: int) -> None:
    """Keep the left red tip icon; redraw the label in the remaining columns.

    AIR cells are only 8px tall — nearest-scale from 12px mush Hangul. Instead
    crop the vertical center of a 10px render (bitmap fonts stay crisp).
    """
    x, w, h = AIR_X, AIR_W, AIR_H
    region = sheet.crop((x, y, x + w, y + h)).convert("RGBA")
    px = region.load()
    for yy in range(h):
        for xx in range(8, w):
            px[xx, yy] = (0, 0, 0, 255)

    gw, gh = glyph.size
    if gh > h:
        top = (gh - h) // 2
        glyph = glyph.crop((0, top, gw, top + h))
        gw, gh = glyph.size
    if gw > w - 9:
        glyph = fit(glyph, w - 9, h)
        gw, gh = glyph.size
    region.alpha_composite(glyph, (9, max(0, (h - gh) // 2)))
    sheet.paste(region, (x, y))


def _extract_nxpk_file(nxpk: Path, rel: str) -> bytes | None:
    data = nxpk.read_bytes()
    if data[:4] != b"NXPK":
        return None
    n = struct.unpack_from("<H", data, 6)[0]
    path_len = 96
    for i in range(n):
        base = 8 + i * (path_len + 8)
        path = data[base : base + path_len].split(b"\0")[0].decode("ascii", "replace")
        off, sz = struct.unpack_from("<II", data, base + path_len)
        if path.lower() == rel.lower():
            return data[off : off + sz]
    return None


def _western_pack_candidates() -> list[Path]:
    """Packs that ship the Aeon Genesis / European hand-drawn UI bitmaps.

    Prefer dedicated EN/FR artifacts — `CaveStory/cavestory.nxpk` is often a
    host copy of the locale under test (ko/ja) and must not be trusted.
    """
    return [
        REPO_ROOT / "sd_content" / "homebrews" / "cavestory.nxpk",
        REPO_ROOT / "sd_content" / "homebrews" / "cavestory_en.nxpk",
        REPO_ROOT / "sd_content" / "homebrews" / "cavestory_fr.nxpk",
        REPO_ROOT / "homebrews" / "cavestory.nxpk",
        REPO_ROOT / "CaveStory" / "cavestory_en.nxpk",
        REPO_ROOT / "CaveStory" / "cavestory_fr.nxpk",
    ]


def _find_western_pack() -> Path | None:
    for p in _western_pack_candidates():
        if p.is_file():
            return p
    return None


def restore_western_resetprompt(tb_rgba: Image.Image) -> None:
    """Copy SPR_RESETPROMPT from EN/FR — KO zip has no hand-drawn replacement.

    JP ships Pixel's bitmap in TextBox.pbm; the Korean patch is scripts-only, so
    synthesizing Hangul looks worse than the European ESC/F1/F2 prompt Windows
    users already see with Aeon Genesis / FR packs.
    """
    nxpk = _find_western_pack()
    blob = _extract_nxpk_file(nxpk, "data/TextBox.pbm") if nxpk else None
    if blob is None:
        print("  [ui] western TextBox.pbm not found — leaving ResetPrompt as-is")
        return
    src = Image.open(io.BytesIO(blob)).convert("RGBA")
    x, y, w, h = RESETPROMPT["x"], RESETPROMPT["y"], RESETPROMPT["w"], RESETPROMPT["h"]
    tb_rgba.paste(src.crop((x, y, x + w, y + h)), (x, y))
    print(f"  [ui] pause prompt ← English ESC/F1/F2 from {nxpk.name}")


def restore_english_title_logo(title_rgba: Image.Image) -> None:
    """Copy the EN SPR_TITLE wordmark (0,0 140×32) into the current Title sheet."""
    nxpk = _find_western_pack()
    blob = _extract_nxpk_file(nxpk, "data/Title.pbm") if nxpk else None
    if blob is None:
        print("  [ui] EN Title.pbm not found — keeping existing logo")
        return
    en = Image.open(io.BytesIO(blob)).convert("RGBA")
    logo = en.crop((0, 0, TITLE["w"], TITLE["h"]))
    title_rgba.paste(logo, (TITLE["x"], TITLE["y"]))
    print(f"  [ui] title logo ← English wordmark from {nxpk.name}")


def localize_ko(root: Path) -> None:
    font12 = ImageFont.truetype(str(ensure_fusion_pixel(12, "ko")), 12)
    try:
        font10 = ImageFont.truetype(str(ensure_fusion_pixel(10, "ko")), 10)
    except SystemExit:
        font10 = font12

    tb_path = root / "data" / "TextBox.pbm"
    title_path = root / "data" / "Title.pbm"
    if not tb_path.is_file() or not title_path.is_file():
        raise SystemExit(f"missing TextBox.pbm or Title.pbm under {root}/data")

    tb, tb_pal = open_sheet(tb_path)
    paste_yesno(
        tb,
        render_text(STRINGS_KO["yesno"], font12, fill=COL_WHITE, shadow=COL_SHADOW),
    )
    air = render_text(STRINGS_KO["air"], font10, fill=COL_WHITE, shadow=COL_SHADOW)
    for y in AIR_YS:
        paste_air(tb, air, y)
    # No Hangul synth — keep Pixel-quality European prompt from EN/FR pack.
    restore_western_resetprompt(tb)

    save_sheet(tb_path, tb, tb_pal)
    print(
        f"  [ui] {tb_path.relative_to(root)} ← "
        f"{STRINGS_KO['yesno']} + {STRINGS_KO['air']} + EN pause prompt"
    )

    title, title_pal = open_sheet(title_path)
    restore_english_title_logo(title)
    for frame_y, key in ((0, "menu0"), (16, "menu1")):
        g = fit(
            render_text(STRINGS_KO[key], font10, fill=COL_TITLE),
            MENU["w"],
            MENU["h"],
        )
        paste_black_centered(title, g, {**MENU, "y": frame_y})
    save_sheet(title_path, title, title_pal)
    print(
        f"  [ui] {title_path.relative_to(root)} ← Cave Story logo + "
        f"{STRINGS_KO['menu0']} / {STRINGS_KO['menu1']}"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("root", nargs="?", default="CaveStory", type=Path)
    ap.add_argument("--locale", default="ko")
    args = ap.parse_args()
    if args.locale != "ko":
        print(f"[UI] locale {args.locale}: no sprite rewrite")
        return 0
    print(f"[UI] localizing sprites in {args.root} (ko)")
    localize_ko(args.root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
