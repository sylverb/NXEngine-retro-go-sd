# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Release tags must match a section heading exactly (for example `v1.0.0`).

## [v1.0.0] - 2026-10-04

Initial Cave Story GWHB release (`PROJECT_KIND=homebrew`).

### Added

- NXEngine Cave Story engine on Retro-Go SD as `CaveStory.bin`.
- Single `cavestory.nxpk` asset pack (8bpp sheets, decrypted TSC, music/SFX)
  flash-cached once and served via XIP.
- CI builds `cavestory.nxpk` from the freeware English zip
  (`https://www.cavestory.one/downloads/cavestoryen.zip`): extract
  `Doukutsu.exe`, synthesize `drum.pcm` / `sndcache.pcm`, pack NXPK.
- Release assets: `CaveStory-<tag>.zip` (`homebrews/*.bin` + `cavestory.nxpk`)
  and `CaveStory-<tag>-debug.zip` (ELF + linker map).
- LUT8 LCD path, title cover packing, saves under `/data/homebrew/cavestory_*`.
- Host build (`make host`) for desktop bring-up against the same pack.
- Fix for flash-cache position hardfaults on large EXTFLASH (XIP pointers no
  longer `free()`’d when the pack sits above 16/32 MiB).

### Install

- Unzip the release archive onto the SD root (creates `/homebrews/CaveStory.bin`
  and `/homebrews/cavestory.nxpk`).
- Optional coverflow override: `/covers/homebrew/CaveStory.img` (JPEG ≤186×100,
  ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
