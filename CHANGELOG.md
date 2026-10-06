# Changelog

## [v1.0.2] - 2026-10-06

### Added

- Nothing.

### Fixed

- Japanese title menu: widen `SPR_MENU` from 40→52 px so `最初から` /
  `続きから` keep the final `ら` (was clipped by the English frame size).

### Changed

- Nothing.

### Install

- Pick a language zip (`CaveStory_en-<tag>.zip`, `_de-…`, `_ja-…`, …) and unzip
  onto the SD root. Several languages may be installed together.
- Saves/settings stay shared: `/data/homebrew/cavestory_settings.dat`,
  `cavestory_profile.dat`.
- Optional coverflow override: `/covers/homebrew/CaveStory_<loc>.img`
  (JPEG ≤186×100, ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
