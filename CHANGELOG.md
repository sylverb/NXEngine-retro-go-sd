# Changelog

All notable changes to this Cave Story GWHB port are documented here.

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Release tags must
match a section heading exactly (for example `v1.0.0`).

When you cut a release:

1. Move items from `[Unreleased]` into a new `## [vX.Y.Z] - YYYY-MM-DD` section.
2. Commit the changelog update.
3. Push the tag: `git tag vX.Y.Z && git push origin vX.Y.Z`

CI reads the matching section and uses it as the GitHub Release notes. Assets
attached to the release:

- `<binary>-<tag>.zip` — SD layout only (`homebrews/` + packed `.bin` + `.nxpk`)
- `<binary>-<tag>-debug.zip` — ELF + linker map (use `arm-none-eabi-addr2line` for crash PC/LR → function/line)

## [Unreleased]

### Fixed

- Hardfault on some devices with the same firmware: NXPK XIP pointers were
  `free()`’d when the circular flash cache placed `cavestory.nxpk` above a
  fixed 16/32 MiB OSPI cutoff. Teardown now uses `gw_pack_ptr_in_pack()` so
  any cache offset (including 64–256 MiB chips) is safe.
- DTCM stack overflow during boot audio init: keep SAI paused through
  `sound_init` / org+pxt load, then `audio_start_playing` afterward.
- Shrink path / log buffers (`PATH_MAX` / `MAXPATHLEN`) and avoid `sprintf` of
  TSC pack keys during ITCM `tsc_init` (firmware `vsprintf` stack cost).
- Full NXPK TOC validation + first/last-byte XIP probe at pack map time;
  clearer “clear flash cache” banner on a bad/stale cache hit.

### Changed

- Load drums into RAM pools instead of pinning XIP slices; org mix buffers
  allocate from `gw_alloc` instead of large static BSS.
- Defer noisy pool/printf spam at boot; log the mapped NXPK absolute address
  for flash-cache diagnosis.

## [v1.0.0] - 2026-10-03

Initial Cave Story GWHB release (`PROJECT_KIND=homebrew`).

### Added

- NXEngine Cave Story engine on Retro-Go SD as `CaveStory.bin`.
- Single `cavestory.nxpk` asset pack (8bpp sheets, decrypted TSC, music/SFX)
  flash-cached once and served via XIP.
- LUT8 LCD path, title cover packing, saves under `/data/homebrew/cavestory_*`.
- Host build (`make host`) for desktop bring-up against the same pack.

### Install

- Copy `CaveStory.bin` and `cavestory.nxpk` to `/homebrews/` on the SD card.
- Optional coverflow override: `/covers/homebrew/CaveStory.img` (JPEG ≤186×100,
  ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.

The release archive contains the ready-to-copy SD layout under `homebrews/`.
