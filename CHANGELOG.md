# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Release tags must match a section heading exactly (for example `v1.0.0`).

## [v1.0.1] - 2026-10-05

### Added

- Nothing.

### Fixed

- Stage-change (TRA) memory growth: Object / Caret / FloatText freelists on bump;
  freeable spritesheet / tileset wrappers and map TSC on AHB with `FlushSheets`.
- Stuck frameskip after long room loads: `common_emu_frame_loop_reset()` at the
  end of `load_stage` (pause menu used to be the only recovery).
- Retro-Go pause menu showing a black background: blit the last game frame in
  the menu `repaint` callback before the firmware darkens and draws chrome.

### Changed

- Release language zips are named `CaveStory-<locale>-<tag>.zip` (e.g.
  `CaveStory-en-v1.0.1.zip`) instead of `CaveStory-<tag>-<locale>.zip`.

### Install

- Pick a language zip (`CaveStory-en-<tag>.zip`, `-de-…`, `-ja-…`, …) and
  unzip onto the SD root (`/homebrews/CaveStory.bin` + `cavestory.nxpk`).
- Optional coverflow override: `/covers/homebrew/CaveStory.img` (JPEG ≤186×100,
  ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
