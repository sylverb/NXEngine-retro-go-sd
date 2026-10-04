#!/usr/bin/env python3
"""Locale catalog for Cave Story freeware packs (cavestory.one / Studio Pixel).

English is a full tree with Doukutsu.exe (music/SFX source). Most fan
translations only ship `data/` overlays — we merge them onto an English
base after extracting org/pxt/wavetable from Doukutsu.exe.

Sources: https://www.cavestory.one/download/cave-story.php
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
    # "overlay" = translated data/ only; needs English base for Doukutsu.exe.
    kind: str
    notes: str = ""


LOCALES: dict[str, Locale] = {
    "en": Locale(
        id="en",
        name="English (Aeon Genesis pre-patched)",
        url=f"{BASE_URL}/cavestoryen.zip",
        nxpk="cavestory.nxpk",
        kind="full",
        notes="Default CI pack.",
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
    # Ready to extend — verify archive layout before enabling in CI:
    # "de": Locale(... Reality Dreamers zip ...),
    # "es": Locale(... Orden rar ...),
}


def get_locale(locale_id: str) -> Locale:
    key = locale_id.strip().lower()
    if key not in LOCALES:
        known = ", ".join(sorted(LOCALES))
        raise SystemExit(f"unknown locale {locale_id!r}; known: {known}")
    return LOCALES[key]


def locale_ids() -> list[str]:
    """Stable order for CI: English first, then the rest alphabetically."""
    ids = sorted(LOCALES)
    if "en" in ids:
        ids.remove("en")
        ids.insert(0, "en")
    return ids


def list_locales() -> None:
    for loc in LOCALES.values():
        print(f"  {loc.id:6}  {loc.nxpk:22}  {loc.kind:8}  {loc.name}")
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
    args = ap.parse_args()
    if args.ids:
        for loc_id in locale_ids():
            print(loc_id)
    else:
        list_locales()
