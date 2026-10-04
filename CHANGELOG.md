# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Release tags must match a section heading exactly (for example `v1.0.0`).

## [v1.0.0] - 2026-10-04

Initial Cave Story GWHB release (`PROJECT_KIND=homebrew`).

### Added

- NXEngine Cave Story engine on Retro-Go SD as `CaveStory.bin`.
- Single `cavestory.nxpk` asset pack (8bpp sheets, decrypted TSC, music/SFX)
  flash-cached once and served via XIP.
- CI builds one `cavestory[_xx].nxpk` per locale (`en`, `de`, `es`, `fi`,
  `fr`, `it`, `nl`, `pt` from `scripts/cavestory_locales.py`): extract
  `Doukutsu.exe`, synthesize `drum.pcm` / `sndcache.pcm`, pack NXPK
  (Western CP1252 accents via extended `smalfont.bmp`).
- Release assets: one SD zip per language
  (`CaveStory-<tag>-en.zip`, `-de.zip`, `-es.zip`, …) each with
  `homebrews/CaveStory.bin` + `homebrews/cavestory.nxpk`, plus
  `CaveStory-<tag>-debug.zip` (ELF + linker map).
- LUT8 LCD path, title cover packing, saves under `/data/homebrew/cavestory_*`.
- Host build (`make host`) for desktop bring-up against the same pack.
- Fix for flash-cache position hardfaults on large EXTFLASH (XIP pointers no
  longer `free()`’d when the pack sits above 16/32 MiB).

### Install

- Pick a language zip (`CaveStory-<tag>-en.zip`, `-de.zip`, `-es.zip`, …) and
  unzip onto the SD root (`/homebrews/CaveStory.bin` + `cavestory.nxpk`).
- Optional coverflow override: `/covers/homebrew/CaveStory.img` (JPEG ≤186×100,
  ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
