# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Release tags must match a section heading exactly (for example `v1.0.0`).

## [v1.0.1] - 2026-10-05

### Added

- Japanese (`ja`) locale packs: Shift-JIS → UTF-8 TSC, used-glyph `cjkfont.dat`
  atlas from Fusion Pixel 12px monospaced (pixel-perfect bake).
- Korean (`ko`) packs in CI: Japanese Doukutsu base + TSC scraped from
  cavestory.one, Fusion Pixel atlas, localized Yes/No/AIR/menu bitmaps, and the
  European ESC pause prompt (KO zip has no hand-drawn sprite).
- Multi-locale homebrews: `CaveStory_<loc>.bin` + `cavestory_<loc>.nxpk` can
  coexist under `/homebrews/`; the GWHB stem selects the pack at boot.

### Fixed

- Stage-change (TRA) memory growth: Object / Caret / FloatText freelists on bump;
  freeable spritesheet / tileset wrappers and map TSC on AHB with `FlushSheets`.
- Stuck frameskip after long room loads: `common_emu_frame_loop_reset()` at the
  end of `load_stage` (pause menu used to be the only recovery).
- Retro-Go pause menu showing a black background: blit the last game frame in
  the menu `repaint` callback before the firmware darkens and draws chrome.
- NXPK path used the GWHB display name (`Cave Story FR`) instead of the SD
  stem (`CaveStory_fr` → `cavestory_fr.nxpk`).
- Boot failures (missing/bad nxpk) no longer spin forever: show a message and
  return to Retro-Go on any button press.
- Japanese/Korean packs: do not extract `org/`/`endpic/` from JP Doukutsu.exe
  (offsets differ → empty Org magic / broken music). Prepare as EN extract +
  locale `data/`; stage captions decode Shift-JIS → UTF-8 for save-select names.
- Resolve `data/../endpic/…` pack paths (Pixel portrait in `sprites.sif`).
- Japanese title menu: strip baked-in 「・」 bullets from `Title.pbm` (NXEngine
  already draws the character cursor).
- CJK save-select / UI text: do not apply Latin `spacing` to Hangul/Kanji
  advances (was crushing 12px glyphs to 5px and looking garbled).
- Org load failures log to the debug console only (no on-screen spam every
  room change when a pack's music extract was bad).

### Changed

- Release zips are named `CaveStory_<locale>-<tag>.zip` and contain the matching
  `CaveStory_<locale>.bin` + `cavestory_<locale>.nxpk` (no rename to a shared
  `cavestory.nxpk`). English pack is `cavestory_en.nxpk`.

### Install

- Pick a language zip (`CaveStory_en-<tag>.zip`, `_de-…`, `_ja-…`, …) and unzip
  onto the SD root. Several languages may be installed together.
- Saves/settings stay shared: `/data/homebrew/cavestory_settings.dat`,
  `cavestory_profile.dat`.
- Optional coverflow override: `/covers/homebrew/CaveStory_<loc>.img`
  (JPEG ≤186×100, ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
