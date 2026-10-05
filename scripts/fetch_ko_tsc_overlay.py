#!/usr/bin/env python3
"""Fetch Korean Cave Story TSC scripts from cavestory.one and overlay a JP tree.

cavestory.one's `cavestory_k.7z` is a Windows PatchProgram (no extractable
`data/`), so CI cannot merge a classic overlay. The same site publishes the
translated scripts as UTF-8 HTML (`/game-info/tsc-script.php/ko/...`) — we pull
those into `CaveStory/data/**/*.tsc` after the Japanese base is prepared.

Usage:
  python3 scripts/fetch_ko_tsc_overlay.py CaveStory
"""
from __future__ import annotations

import argparse
import html
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://www.cavestory.one"
INDEX = f"{BASE}/game-info/tsc-script.php/ko"
UA = "NXEngine-retro-go-sd/1.0 (ko TSC overlay; +https://github.com/)"


def _get(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=90) as resp:
        return resp.read().decode("utf-8", "replace")


def list_script_slugs() -> list[str]:
    page = _get(INDEX)
    seen: set[str] = set()
    slugs: list[str] = []
    for slug in re.findall(
        r"tsc-script\.php/ko/([a-zA-Z0-9_]+)", page, flags=re.IGNORECASE
    ):
        key = slug.lower()
        if key in seen:
            continue
        seen.add(key)
        slugs.append(slug)
    if not slugs:
        raise SystemExit(f"no script links found at {INDEX}")
    return slugs


def extract_script_body(page_html: str) -> str:
    for pat in (
        r"<pre[^>]*>(.*?)</pre>",
        r"<textarea[^>]*>(.*?)</textarea>",
        r"<code[^>]*>(.*?)</code>",
    ):
        ms = re.findall(pat, page_html, flags=re.IGNORECASE | re.DOTALL)
        if not ms:
            continue
        # Prefer the longest block (nav crumbs can inject tiny <code>).
        body = max(ms, key=len)
        text = html.unescape(body)
        # Drop HTML leftovers if any nested tags slipped through.
        text = re.sub(r"<[^>]+>", "", text)
        return text
    raise ValueError("no <pre>/<textarea>/<code> script body")


def tsc_path_map(tree: Path) -> dict[str, Path]:
    """stem.lower() → path under tree (e.g. 'head' → data/Head.tsc)."""
    data = tree / "data"
    if not data.is_dir():
        raise SystemExit(f"missing {data} — prepare Japanese base first")
    out: dict[str, Path] = {}
    for path in data.rglob("*.tsc"):
        out[path.stem.lower()] = path
    if not out:
        raise SystemExit(f"no .tsc under {data}")
    return out


def apply_ko_tsc_overlay(tree: Path, *, quiet: bool = False) -> int:
    """Overwrite tree TSC files with Korean UTF-8 plaintext from cavestory.one.

    Returns the number of scripts written.
    """
    paths = tsc_path_map(tree)
    slugs = list_script_slugs()
    if not quiet:
        print(f"[ko] fetching {len(slugs)} TSC from {INDEX}")

    written = 0
    missing: list[str] = []
    empty: list[str] = []
    for slug in slugs:
        key = slug.lower()
        dest = paths.get(key)
        if dest is None:
            missing.append(slug)
            continue
        url = f"{INDEX}/{slug}"
        try:
            page = _get(url)
            body = extract_script_body(page)
        except (urllib.error.URLError, TimeoutError, ValueError) as exc:
            raise SystemExit(f"failed to fetch {url}: {exc}") from exc
        if len(body.strip()) < 8:
            empty.append(slug)
            continue
        # Preserve CRLF style from the site; pack converts to UTF-8 cleartext.
        dest.write_bytes(body.encode("utf-8"))
        written += 1

    if missing:
        raise SystemExit(
            f"web slugs with no matching .tsc on disk ({len(missing)}): "
            + ", ".join(missing[:12])
            + ("…" if len(missing) > 12 else "")
        )
    if empty:
        raise SystemExit(f"empty script bodies: {', '.join(empty)}")
    if written != len(paths):
        # Disk may have scripts the web index omitted — warn, do not fail if
        # we wrote every listed slug (JP leftovers stay until pack).
        leftover = sorted(set(paths) - {s.lower() for s in slugs})
        if leftover and not quiet:
            print(
                f"[ko] warning: {len(leftover)} on-disk TSC not on web index "
                f"(kept JP): {', '.join(leftover[:8])}"
                + ("…" if len(leftover) > 8 else "")
            )
    if not quiet:
        print(f"[ko] wrote {written} Korean UTF-8 TSC under {tree / 'data'}")
    return written


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "tree",
        nargs="?",
        default="CaveStory",
        type=Path,
        help="Cave Story tree with data/*.tsc (default: CaveStory)",
    )
    args = ap.parse_args()
    apply_ko_tsc_overlay(args.tree.resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
