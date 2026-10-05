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
    "doconffr.exe",
    "orgview.exe",
    "readme.txt",
    "manual.html",
    "config.dat",
    "settings.dat",
    "debug.txt",
    "thumbs.db",
    "cavestory.nxpk",
    "cavestory_fr.nxpk",
    "cavestory_ja.nxpk",
    "cavestory_ko.nxpk",
    "doukutsu",
    "doukutsu.bin",
    "doconfigure",
}
EXCLUDE_DIR_NAMES = {"manual", "doc", "docs", ".git"}
EXCLUDE_SUFFIXES = {".exe", ".bin", ".old", ".jpg", ".png", ".html", ".so", ".so.0"}


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
    if name.endswith(".nxpk"):
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


def tsc_encrypt_bytes(plain: bytes) -> bytes:
    """Inverse of tsc_decrypt; mid-byte is the key (= original mid plaintext)."""
    if not plain:
        return plain
    buf = bytearray(plain)
    keypos = len(buf) // 2
    dkey = buf[keypos]
    for i in range(keypos):
        buf[i] = (buf[i] + dkey) & 0xFF
    for i in range(keypos + 1, len(buf)):
        buf[i] = (buf[i] + dkey) & 0xFF
    return bytes(buf)


def normalize_rel(path: Path) -> str:
    return path.as_posix()


def _looks_like_plaintext_tsc(text: str) -> bool:
    """True if text is an unencrypted Cave Story script (has #events and <CMDs)."""
    return "\x00" not in text and "#" in text and "<" in text


# stage.dat packed record (matches third_party/nxengine/maprecord.h + map.cpp)
_STAGE_REC = 73  # filename[32] + stagename[35] + 6×u8
_STAGE_NAME_OFF = 32
_STAGE_NAME_LEN = 35


def convert_stage_dat_names_to_utf8(
    data: bytes,
    name_encoding: str = "cp932",
    overrides: dict[str, str] | None = None,
) -> bytes:
    """Rewrite stagename[] fields from locale encoding (or overrides) to UTF-8.

    CJK packs draw map names via utf8_next + cjkfont; Shift-JIS captions become
    mojibake (e.g. スタート地点 → �X�^�[�g�n�_) if left unconverted.
    """
    if not data:
        return data
    n = data[0]
    out = bytearray(data)
    for i in range(n):
        off = 1 + i * _STAGE_REC
        if off + _STAGE_REC > len(out):
            break
        fname = bytes(out[off : off + 32]).split(b"\0")[0].decode("ascii", "ignore")
        raw = bytes(
            out[off + _STAGE_NAME_OFF : off + _STAGE_NAME_OFF + _STAGE_NAME_LEN]
        ).split(b"\0")[0]
        if not raw and not (overrides and fname in overrides):
            continue

        if overrides and fname in overrides:
            text = overrides[fname]
        else:
            try:
                text = raw.decode("utf-8")
                # Already UTF-8 (re-pack) — keep.
            except UnicodeDecodeError:
                text = raw.decode(name_encoding, errors="replace")

        enc = text.encode("utf-8")[: _STAGE_NAME_LEN - 1]
        out[off + _STAGE_NAME_OFF : off + _STAGE_NAME_OFF + _STAGE_NAME_LEN] = (
            b"\0" * _STAGE_NAME_LEN
        )
        out[off + _STAGE_NAME_OFF : off + _STAGE_NAME_OFF + len(enc)] = enc
    return bytes(out)


def load_ko_stage_name_overrides() -> dict[str, str]:
    """Best-effort: Korean captions from cavestory.one script index (filename → name)."""
    import html
    import re
    import urllib.request

    url = "https://www.cavestory.one/game-info/tsc-script.php/ko"
    req = urllib.request.Request(
        url, headers={"User-Agent": "NXEngine-pack/1.0 (stage.dat names)"}
    )
    try:
        page = urllib.request.urlopen(req, timeout=60).read().decode("utf-8", "replace")
    except Exception as exc:
        print(f"[CJK] ko stage-name fetch failed ({exc}); using JP→UTF-8 only")
        return {}

    # <a href=".../ko/start">Stage/Start.tsc (스타트 지점)</a>
    overrides: dict[str, str] = {}
    for m in re.finditer(
        r">Stage/([A-Za-z0-9_]+)\.tsc\s*\(([^)]+)\)<", page
    ):
        fname, caption = m.group(1), html.unescape(m.group(2)).strip()
        if caption:
            overrides[fname] = caption
    print(f"[CJK] ko stage-name overrides: {len(overrides)}")
    return overrides


def convert_tsc_to_utf8(data: bytes, encoding: str) -> bytes:
    """Decrypt → decode locale encoding → UTF-8 cleartext for NXPK.

    Idempotent: plaintext UTF-8 TSC (scraped / re-pack) is returned as-is.
    Must NOT run the Doukutsu subtract-cipher on plaintext — that turns valid
    UTF-8 Hangul/Kanji into garbage that still often decodes as UTF-8.
    """
    # Already plaintext UTF-8 (e.g. cavestory.one scrape, prior pack rewrite).
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        text = None
    if text is not None and _looks_like_plaintext_tsc(text):
        return text.encode("utf-8")

    dec = tsc_decrypt_bytes(data)
    try:
        text = dec.decode("utf-8")
    except UnicodeDecodeError:
        text = None
    if text is not None and _looks_like_plaintext_tsc(text):
        return text.encode("utf-8")

    text = dec.decode(encoding, errors="replace")
    return text.encode("utf-8")


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


def collect_entries(
    root: Path,
    *,
    text_encoding: str = "cp1252",
    cjk: bool = False,
    rewrite_disk_tsc: bool = False,
) -> list[tuple[str, bytes]]:
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
            raw = path.read_bytes()
            if cjk:
                data = convert_tsc_to_utf8(raw, text_encoding)
                kind = "tsc-utf8"
                if rewrite_disk_tsc:
                    # Host fileopen path: encrypted UTF-8 (identity-safe mid key).
                    path.write_bytes(tsc_encrypt_bytes(data))
            else:
                data = tsc_decrypt_bytes(raw)
                kind = "tsc-clear"
        elif path.name.lower() == "stage.dat" and cjk:
            # Already rewritten on disk in main(); pack the UTF-8 captions.
            data = path.read_bytes()
            kind = "stage-utf8"
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


def bake_cjk_for_root(root: Path, locale_id: str) -> Path:
    """Run bake_cjk_atlas.py against UTF-8 (or legacy) TSC; return cjkfont.dat path."""
    import subprocess

    script = Path(__file__).resolve().parent / "bake_cjk_atlas.py"
    out = root / "cjkfont.dat"
    # TSC may already be rewritten to UTF-8 on disk when rewrite_disk_tsc ran first.
    # Bake scans files — prefer --codepoints-from-utf8 after conversion.
    cmd = [
        sys.executable,
        str(script),
        str(root),
        "-o",
        str(out),
        "--locale",
        locale_id,
        "--codepoints-from-utf8",
    ]
    print(f"[CJK] baking atlas via {' '.join(cmd)}")
    subprocess.run(cmd, check=True)
    if not out.is_file():
        raise SystemExit(f"cjkfont bake failed: missing {out}")
    return out


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
        help="Also copy to sd_content/homebrews/<output-name>",
    )
    ap.add_argument(
        "--locale",
        default=None,
        help="locale id from cavestory_locales.py (sets default -o name)",
    )
    args = ap.parse_args()
    root = Path(args.root)
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 1

    default_name = "cavestory.nxpk"
    loc = None
    if args.locale:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from cavestory_locales import get_locale

        loc = get_locale(args.locale)
        default_name = loc.nxpk

    out = Path(args.output) if args.output else root / default_name
    print(f"packing {root} → {out}")

    text_encoding = loc.text_encoding if loc else "cp1252"
    cjk = bool(loc.cjk) if loc else False

    # CJK: convert TSC + stage.dat names → UTF-8 on disk first so the atlas
    # scanner sees Unicode, bake cjkfont.dat, then pack.
    if cjk:
        print(f"[CJK] converting TSC {text_encoding} → UTF-8")
        for path in sorted(root.rglob("*.tsc")):
            rel = path.relative_to(root)
            if should_skip(rel):
                continue
            utf8 = convert_tsc_to_utf8(path.read_bytes(), text_encoding)
            path.write_bytes(tsc_encrypt_bytes(utf8))

        # Base game stage.dat captions are Shift-JIS (JP 1.0.0.6).
        stage_path = root / "stage.dat"
        if stage_path.is_file():
            overrides = (
                load_ko_stage_name_overrides() if loc and loc.id == "ko" else None
            )
            converted = convert_stage_dat_names_to_utf8(
                stage_path.read_bytes(),
                name_encoding="cp932",
                overrides=overrides,
            )
            stage_path.write_bytes(converted)
            print(f"[CJK] stage.dat names → UTF-8 ({stage_path.stat().st_size} bytes)")

        # Yes/No, AIR, title menu are bitmaps — rewrite for Korean.
        if loc and loc.id == "ko":
            import subprocess

            ui = Path(__file__).resolve().parent / "localize_ui_sprites.py"
            subprocess.run(
                [sys.executable, str(ui), str(root), "--locale", "ko"],
                check=True,
            )

        bake_cjk_for_root(root, loc.id if loc else "ja")

    entries = collect_entries(
        root,
        text_encoding="utf-8" if cjk else text_encoding,
        cjk=cjk,
        rewrite_disk_tsc=False,
    )
    if not entries:
        print("error: no files packed", file=sys.stderr)
        return 1
    write_nxpk(entries, out)

    if args.also_sd:
        sd = Path("sd_content/homebrews") / out.name
        sd.parent.mkdir(parents=True, exist_ok=True)
        sd.write_bytes(out.read_bytes())
        print(f"copied → {sd}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
