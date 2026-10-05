#!/usr/bin/env python3
"""Locale catalog for Cave Story freeware packs (cavestory.one / Studio Pixel).

English is a full tree with Doukutsu.exe (music/SFX source). Most fan
translations only ship `data/` overlays — we merge them onto an English
base after extracting org/pxt/wavetable from Doukutsu.exe.

Sources: https://www.cavestory.one/download/cave-story.php

CI packs every locale here (`make ci-assets-all`). Prefer zip/7z overlays;
`.rar` works when `bsdtar`/`unrar`/`unar` is available (CI installs unrar).
CJK (ja/ko): pack-time UTF-8 + used-glyph cjkfont.dat from Fusion Pixel
12px monospaced (TakWolf; auto-fetched at bake). Korean TSC come from
cavestory.one’s script pages (PatchProgram zip is not CI-usable).
Cyrillic / CP1254 skipped.
"""
from __future__ import annotations

from dataclasses import dataclass


BASE_URL = "https://www.cavestory.one/downloads"


@dataclass(frozen=True)
class Locale:
    id: str
    name: str
    url: str
    nxpk: str
    # "full" = archive contains Doukutsu.exe (+ data/).
    # "overlay" = translated data/ only; needs a full base for Doukutsu.exe.
    kind: str
    notes: str = ""
    # Script encoding of on-disk .tsc before pack-time UTF-8 conversion.
    # Latin fans are CP1252 (opaque 8-bit; left as-is). ja=cp932, ko=cp949.
    text_encoding: str = "cp1252"
    # For overlays: which full locale supplies Doukutsu.exe / audio extract.
    base_locale: str = "en"
    # Bake cjkfont.dat and convert TSC → UTF-8 at pack time.
    cjk: bool = False
    # Include in `make ci-assets-all` / release matrix. False when the
    # published archive is a Windows-only patcher (needs a pre-built data/).
    ci: bool = True


LOCALES: dict[str, Locale] = {
    "en": Locale(
        id="en",
        name="English (Aeon Genesis pre-patched)",
        url=f"{BASE_URL}/cavestoryen.zip",
        nxpk="cavestory_en.nxpk",
        kind="full",
        notes="Default CI pack.",
    ),
    "de": Locale(
        id="de",
        name="German (Reality Dreamers 1.00)",
        url=f"{BASE_URL}/Cave_Story_DE_v1.00_Reality_Dreamers_2018.zip",
        nxpk="cavestory_de.nxpk",
        kind="overlay",
        notes="JP→DE; ships its own Doukutsu.exe but we still overlay data/ on EN audio.",
    ),
    "es": Locale(
        id="es",
        name="Spanish (Orden 1.22)",
        url=f"{BASE_URL}/Cave%20Story%20ESP%201.22.rar",
        nxpk="cavestory_es.nxpk",
        kind="overlay",
        notes="Requires unrar/bsdtar for .rar (CI installs unrar).",
    ),
    "fi": Locale(
        id="fi",
        name="Finnish (JP32 / Luola Tarina)",
        url=f"{BASE_URL}/Luola%20Tarina.zip",
        nxpk="cavestory_fi.nxpk",
        kind="overlay",
    ),
    "fr": Locale(
        id="fr",
        name="French (Max le Fou 1.21)",
        # Linux .7z is a plain data tree (no NSIS installer). Windows zip
        # only contains csfrsetup.exe (Nullsoft) — prefer Linux for CI/make.
        url=f"{BASE_URL}/Cave%20Story%20FR%201.21%20Linux.7z",
        nxpk="cavestory_fr.nxpk",
        kind="overlay",
        notes="Overlay onto English base; music/SFX from Doukutsu.exe (EN).",
    ),
    "it": Locale(
        id="it",
        name="Italian (Simon M. v2.0)",
        url=f"{BASE_URL}/Cave_Story_Italian_v2.0.zip",
        nxpk="cavestory_it.nxpk",
        kind="overlay",
    ),
    "ja": Locale(
        id="ja",
        name="Japanese (Studio Pixel 1.0.0.6)",
        url=f"{BASE_URL}/dou_1006.zip",
        nxpk="cavestory_ja.nxpk",
        # JP Doukutsu.exe ORG/endpic offsets differ from EN — overlay JP data/
        # onto an English extract so music/SFX/credits stay valid.
        kind="overlay",
        text_encoding="cp932",
        cjk=True,
        base_locale="en",
        notes=(
            "JP data/ on EN Doukutsu extract (org/pxt/endpic). "
            "Shift-JIS TSC + stage captions → UTF-8 + cjkfont.dat."
        ),
    ),
    "ko": Locale(
        id="ko",
        name="Korean (Anonymous / romhacking 2147)",
        # Published .7z is a Windows PatchProgram — prepare scrapes UTF-8 TSC
        # from cavestory.one instead (see scripts/fetch_ko_tsc_overlay.py).
        url=f"{BASE_URL}/cavestory_k.7z",
        nxpk="cavestory_ko.nxpk",
        kind="overlay",
        text_encoding="utf-8",
        base_locale="en",
        cjk=True,
        ci=True,
        notes=(
            "EN Doukutsu extract (music) + JP data/ sprites + Korean TSC from "
            "cavestory.one; pack bakes Fusion Pixel cjkfont.dat + KO UI bitmaps. "
            "Optional `--archive` with a real data/ tree still works."
        ),
    ),
    "nl": Locale(
        id="nl",
        name="Dutch (Ian Noah 2017)",
        url="https://cavestorynl.weebly.com/uploads/4/8/5/2/48528295/cavestorynl29-9-2017.zip",
        nxpk="cavestory_nl.nxpk",
        kind="overlay",
        notes="Hosted on weebly (not cavestory.one).",
    ),
    "pt": Locale(
        id="pt",
        name="Portuguese Portugal (André Silva)",
        url=f"{BASE_URL}/Cave_Story_pt_PT.7z",
        nxpk="cavestory_pt.nxpk",
        kind="overlay",
    ),
}


def get_locale(locale_id: str) -> Locale:
    key = locale_id.strip().lower()
    if key not in LOCALES:
        known = ", ".join(sorted(LOCALES))
        raise SystemExit(f"unknown locale {locale_id!r}; known: {known}")
    return LOCALES[key]


def hb_bin_name(locale_id: str) -> str:
    """GWHB filename on SD: CaveStory_<loc>.bin"""
    return f"CaveStory_{locale_id.strip().lower()}.bin"


def hb_display_name(locale_id: str) -> str:
    """Launcher display name: Cave Story EN / Cave Story FR / …"""
    return f"Cave Story {locale_id.strip().upper()}"


def locale_ids(*, ci_only: bool = False) -> list[str]:
    """Stable order: English first, then the rest alphabetically."""
    ids = sorted(LOCALES)
    if "en" in ids:
        ids.remove("en")
        ids.insert(0, "en")
    if ci_only:
        ids = [i for i in ids if LOCALES[i].ci]
    return ids


def list_locales() -> None:
    for loc_id in locale_ids():
        loc = LOCALES[loc_id]
        flags = []
        if loc.cjk:
            flags.append("cjk")
        if not loc.ci:
            flags.append("no-ci")
        flag_s = f"  [{', '.join(flags)}]" if flags else ""
        print(f"  {loc.id:6}  {loc.nxpk:22}  {loc.kind:8}  {loc.name}{flag_s}")
        if loc.notes:
            print(f"          {loc.notes}")


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--ids",
        action="store_true",
        help="print locale ids one per line (CI/Make)",
    )
    ap.add_argument(
        "--ci",
        action="store_true",
        help="with --ids, only locales included in ci-assets-all",
    )
    args = ap.parse_args()
    if args.ids:
        for loc_id in locale_ids(ci_only=args.ci):
            print(loc_id)
    else:
        list_locales()
