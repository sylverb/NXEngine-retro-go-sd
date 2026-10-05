#!/usr/bin/env python3
"""Stage Retro-Go SD release assets for the active project kind.

Reads PROJECT_KIND from the root Makefile, builds:
  1. Per-locale SD install zips: CaveStory_<locale>-<tag>.zip
        → homebrews/CaveStory_<locale>.bin
        → homebrews/cavestory_<locale>.nxpk
  2. Debug symbols zip: CaveStory-<tag>-debug.zip → ELF, map, README

Extracts release notes from CHANGELOG.md for the requested tag.

Usage:
  python3 scripts/stage_release.py --tag v1.0.0 --out release \\
      --elf build/homebrew/cavestory_core.elf --map build/homebrew/cavestory_core.map \\
      --bin-locale en=sd_content/homebrews/CaveStory_en.bin \\
      --nxpk-locale en=sd_content/homebrews/cavestory_en.nxpk \\
      --bin-locale fr=sd_content/homebrews/CaveStory_fr.bin \\
      --nxpk-locale fr=sd_content/homebrews/cavestory_fr.nxpk
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "Makefile"
DEFAULT_CHANGELOG = ROOT / "CHANGELOG.md"
DEBUG_README = ROOT / "scripts" / "DEBUG_README.md"

MAKE_VARS = (
    "PROJECT_KIND",
    "PACKED_BIN",
    "CORE_NAME",
    "DOCKER_IMAGE",
    "TARGET_ELF",
    "TARGET_MAP",
)

HEADING_RE = re.compile(
    r"^##\s*(?:\[(?P<bracket>[^\]]+)\]|(?P<plain>[^\s#]+))(?:\s*-\s*(?P<date>.+))?\s*$",
    re.MULTILINE,
)

sys.path.insert(0, str(ROOT / "scripts"))
from cavestory_locales import hb_bin_name, get_locale  # noqa: E402


def read_make_vars() -> dict[str, str]:
    cmd = ["make", "-f", str(MAKEFILE), "--no-print-directory"]
    for var in MAKE_VARS:
        cmd.append(f"print-{var}")

    try:
        # Keep stderr separate — Makefile $(warning) lines must not pollute values.
        proc = subprocess.run(
            cmd,
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
    except OSError as exc:
        raise SystemExit(f"failed to run make: {exc}") from exc

    if proc.returncode != 0:
        sys.stderr.write(proc.stderr or proc.stdout or "")
        raise SystemExit(f"failed to read Makefile variables from {MAKEFILE}")

    if proc.stderr:
        sys.stderr.write(proc.stderr)

    values = [line for line in proc.stdout.splitlines() if line.strip()]
    # Defensive: if anything else leaked to stdout, keep the last N lines.
    if len(values) > len(MAKE_VARS):
        values = values[-len(MAKE_VARS) :]
    if len(values) != len(MAKE_VARS):
        raise SystemExit(
            f"expected {len(MAKE_VARS)} Makefile values, got {len(values)}:\n{proc.stdout}"
        )
    return dict(zip(MAKE_VARS, values, strict=True))


def sd_subdir(project_kind: str) -> str:
    if project_kind == "core":
        return "cores"
    if project_kind == "homebrew":
        return "homebrews"
    raise SystemExit(f"unsupported PROJECT_KIND: {project_kind!r} (expected core or homebrew)")


def slug(tag: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "-", tag).strip("-") or "release"


def extract_changelog_section(changelog_path: Path, tag: str) -> str:
    if not changelog_path.is_file():
        raise SystemExit(f"CHANGELOG not found: {changelog_path}")

    text = changelog_path.read_text(encoding="utf-8")
    matches = list(HEADING_RE.finditer(text))
    for index, match in enumerate(matches):
        version = (match.group("bracket") or match.group("plain") or "").strip()
        if version != tag:
            continue
        start = match.end()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        body = text[start:end].strip()
        if not body:
            raise SystemExit(f"CHANGELOG section for {tag!r} is empty")
        return body

    raise SystemExit(
        "no CHANGELOG section for tag "
        f"{tag!r}; add '## [{tag}] - YYYY-MM-DD' before pushing the tag"
    )


def parse_id_path(spec: str, flag: str) -> tuple[str, Path]:
    if "=" not in spec:
        raise SystemExit(
            f"invalid {flag} {spec!r}; expected ID=PATH "
            f"(e.g. fr=sd_content/homebrews/cavestory_fr.nxpk)"
        )
    loc_id, path_s = spec.split("=", 1)
    loc_id = loc_id.strip().lower()
    path = Path(path_s.strip())
    if not loc_id or not path_s.strip():
        raise SystemExit(f"invalid {flag} {spec!r}; expected ID=PATH")
    if not path.is_absolute():
        path = ROOT / path
    return loc_id, path


def write_zip(archive: Path, members: list[tuple[Path, str]]) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for src, arcname in members:
            zf.write(src, arcname)


def build_release_notes(
    *,
    tag: str,
    changelog_body: str,
    project_kind: str,
    sd_dir: str,
    core_name: str,
    docker_image: str,
    locale_archives: list[tuple[str, str, str, str]],
    debug_archive_name: str,
) -> str:
    sdk_version = (ROOT / "SDK_VERSION").read_text(encoding="utf-8").strip()

    lines = [
        f"# {tag}",
        "",
        changelog_body,
        "",
        "---",
        "",
        f"- Project kind: `{project_kind}`",
        f"- Core id: `{core_name}`",
        "- Language packs (unzip one onto the SD root; several may coexist):",
    ]
    for loc_id, archive_name, bin_name, nxpk_name in locale_archives:
        lines.append(
            f"  - `{archive_name}` — `{loc_id}` → `/{sd_dir}/{bin_name}` + "
            f"`/{sd_dir}/{nxpk_name}`"
        )
    lines += [
        f"- Debug archive: `{debug_archive_name}` (ELF + linker map)",
        f"- Built with: `{docker_image}`",
        f"- SDK: `{sdk_version}`",
        "",
        "Crash PC/LR → source (needs `arm-none-eabi-addr2line`):",
        "",
        "```bash",
        f"unzip {debug_archive_name}",
        "arm-none-eabi-addr2line -e <name>_core.elf -f -C -a 0x<PC> 0x<LR>",
        "```",
        "",
        "Or from a checkout of this repo: `python3 scripts/resolve_addr.py --elf …`",
        "",
    ]
    return "\n".join(lines) + "\n"


def stage_release(
    *,
    tag: str,
    out_dir: Path,
    changelog_path: Path,
    docker_image: str | None,
    elf_path: Path | None,
    map_path: Path | None,
    locale_bins: dict[str, Path],
    locale_nxpks: dict[str, Path],
) -> None:
    cfg = read_make_vars()
    project_kind = cfg["PROJECT_KIND"]
    core_name = cfg["CORE_NAME"]
    resolved_docker = docker_image or cfg.get("DOCKER_IMAGE") or "sylverb/retro-go-sd-builder:v1.5"

    if not locale_nxpks:
        raise SystemExit("need at least one --nxpk-locale ID=PATH (or --nxpk)")

    missing_bins = sorted(set(locale_nxpks) - set(locale_bins))
    if missing_bins:
        raise SystemExit(
            "missing --bin-locale for: "
            + ", ".join(missing_bins)
            + " (or place CaveStory_<loc>.bin next to the nxpk / under --bins-dir)"
        )
    extra_bins = sorted(set(locale_bins) - set(locale_nxpks))
    if extra_bins:
        raise SystemExit(f"--bin-locale without matching --nxpk-locale: {', '.join(extra_bins)}")

    elf = elf_path or (ROOT / cfg["TARGET_ELF"])
    if not elf.is_absolute():
        elf = ROOT / elf
    map_file = map_path or (ROOT / cfg["TARGET_MAP"])
    if not map_file.is_absolute():
        map_file = ROOT / map_file

    if not elf.is_file():
        raise SystemExit(f"ELF not found: {elf}")
    if not map_file.is_file():
        raise SystemExit(f"linker map not found: {map_file}")
    if not DEBUG_README.is_file():
        raise SystemExit(f"debug readme not found: {DEBUG_README}")

    for loc_id, nxpk in locale_nxpks.items():
        if not nxpk.is_file():
            raise SystemExit(f"NXPK not found for locale {loc_id}: {nxpk}")
    for loc_id, bin_p in locale_bins.items():
        if not bin_p.is_file():
            raise SystemExit(f"GWHB not found for locale {loc_id}: {bin_p}")

    changelog_body = extract_changelog_section(changelog_path, tag)

    sd_dir = sd_subdir(project_kind)
    out_dir.mkdir(parents=True, exist_ok=True)
    sd_root = out_dir / sd_dir
    sd_root.mkdir(parents=True, exist_ok=True)

    tag_slug = slug(tag)
    locale_archives: list[tuple[str, str, str, str]] = []
    release_files: list[Path] = []

    for loc_id in sorted(locale_nxpks):
        bin_src = locale_bins[loc_id]
        nxpk_src = locale_nxpks[loc_id]
        bin_name = hb_bin_name(loc_id)
        nxpk_name = get_locale(loc_id).nxpk

        staged_bin = sd_root / bin_name
        staged_nxpk = sd_root / nxpk_name
        shutil.copy2(bin_src, staged_bin)
        shutil.copy2(nxpk_src, staged_nxpk)

        archive_name = f"CaveStory_{loc_id}-{tag_slug}.zip"
        archive_path = out_dir / archive_name
        write_zip(
            archive_path,
            [
                (staged_bin, f"{sd_dir}/{bin_name}"),
                (staged_nxpk, f"{sd_dir}/{nxpk_name}"),
            ],
        )
        locale_archives.append((loc_id, archive_name, bin_name, nxpk_name))
        release_files.append(archive_path)
        print(f"archive[{loc_id}]={archive_path}")

    debug_archive_name = f"CaveStory-{tag_slug}-debug.zip"
    debug_archive_path = out_dir / debug_archive_name
    write_zip(
        debug_archive_path,
        [
            (elf, elf.name),
            (map_file, map_file.name),
            (DEBUG_README, "README.md"),
        ],
    )
    release_files.append(debug_archive_path)

    notes_path = out_dir / "RELEASE_NOTES.md"
    notes_path.write_text(
        build_release_notes(
            tag=tag,
            changelog_body=changelog_body,
            project_kind=project_kind,
            sd_dir=sd_dir,
            core_name=core_name,
            docker_image=resolved_docker,
            locale_archives=locale_archives,
            debug_archive_name=debug_archive_name,
        ),
        encoding="utf-8",
    )

    files_path = out_dir / "release-files.txt"
    files_path.write_text(
        "\n".join(p.name for p in release_files) + "\n",
        encoding="utf-8",
    )

    print(f"release_title={tag}")
    print(f"project_kind={project_kind}")
    print(f"debug_archive={debug_archive_path}")
    print(f"notes={notes_path}")
    print(f"files={files_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--bin",
        dest="bin_path",
        type=Path,
        help="legacy single GWHB used as fallback for every locale missing --bin-locale",
    )
    parser.add_argument(
        "--bin-locale",
        action="append",
        default=[],
        metavar="ID=PATH",
        help="locale GWHB to include (repeatable). Zip ships CaveStory_<id>.bin.",
    )
    parser.add_argument(
        "--bins-dir",
        type=Path,
        help="directory containing CaveStory_<loc>.bin (fills missing --bin-locale)",
    )
    parser.add_argument(
        "--elf",
        dest="elf_path",
        type=Path,
        help="linked ELF with debug symbols (default: TARGET_ELF from Makefile)",
    )
    parser.add_argument(
        "--map",
        dest="map_path",
        type=Path,
        help="linker map (default: TARGET_MAP from Makefile)",
    )
    parser.add_argument("--tag", required=True, help="release tag (e.g. v1.0.0)")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("release"),
        help="output directory (default: release/)",
    )
    parser.add_argument(
        "--changelog",
        type=Path,
        default=DEFAULT_CHANGELOG,
        help="changelog file (default: CHANGELOG.md)",
    )
    parser.add_argument(
        "--docker-image",
        help="builder image string for release notes (default: Makefile DOCKER_IMAGE)",
    )
    parser.add_argument(
        "--nxpk-locale",
        action="append",
        default=[],
        metavar="ID=PATH",
        help="locale pack to include (repeatable). Zip ships cavestory_<id>.nxpk.",
    )
    parser.add_argument(
        "--nxpk",
        dest="nxpk_path",
        type=Path,
        help="shorthand for --nxpk-locale en=PATH (legacy single-pack releases)",
    )
    args = parser.parse_args()

    changelog_path = args.changelog
    if not changelog_path.is_absolute():
        changelog_path = ROOT / changelog_path

    locale_nxpks: dict[str, Path] = {}
    for spec in args.nxpk_locale:
        loc_id, path = parse_id_path(spec, "--nxpk-locale")
        locale_nxpks[loc_id] = path
    if args.nxpk_path is not None:
        nxpk = args.nxpk_path if args.nxpk_path.is_absolute() else (ROOT / args.nxpk_path)
        locale_nxpks.setdefault("en", nxpk)

    locale_bins: dict[str, Path] = {}
    for spec in args.bin_locale:
        loc_id, path = parse_id_path(spec, "--bin-locale")
        locale_bins[loc_id] = path

    bins_dir = args.bins_dir
    if bins_dir is not None and not bins_dir.is_absolute():
        bins_dir = ROOT / bins_dir

    legacy_bin = args.bin_path
    if legacy_bin is not None and not legacy_bin.is_absolute():
        legacy_bin = ROOT / legacy_bin

    for loc_id in locale_nxpks:
        if loc_id in locale_bins:
            continue
        cand = None
        if bins_dir is not None:
            cand = bins_dir / hb_bin_name(loc_id)
        if cand is None or not cand.is_file():
            # Same directory as the nxpk (sd_content/homebrews/).
            cand = locale_nxpks[loc_id].with_name(hb_bin_name(loc_id))
        if cand.is_file():
            locale_bins[loc_id] = cand
        elif legacy_bin is not None and legacy_bin.is_file():
            locale_bins[loc_id] = legacy_bin

    stage_release(
        tag=args.tag,
        out_dir=(ROOT / args.out).resolve() if not args.out.is_absolute() else args.out,
        changelog_path=changelog_path,
        docker_image=args.docker_image,
        elf_path=args.elf_path,
        map_path=args.map_path,
        locale_bins=locale_bins,
        locale_nxpks=locale_nxpks,
    )


if __name__ == "__main__":
    main()
