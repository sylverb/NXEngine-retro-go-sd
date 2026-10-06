# Changelog

## [v1.0.2] - 2026-10-06

### Added

- Nothing.

### Fixed

- Japanese title menu: fix missing final `ら` (was clipped by the English frame size).

### Changed

- Controls: GAME release = inventory (unless GAME+Left/Right was used for
  weapons), TIME = map (Escape removed — double-press used to hard-fault).
  Zelda X/Y = prev/next weapon; Mario uses GAME+Left / GAME+Right. Inventory
  KEYDOWN stays sticky so re-entrant pad polls still reach `justpushed`.

### Install

- Pick a language zip (`CaveStory_en-<tag>.zip`, `_de-…`, `_ja-…`, …) and unzip
  onto the SD root. Several languages may be installed together.
- Saves/settings stay shared: `/data/homebrew/cavestory_settings.dat`,
  `cavestory_profile.dat`.
- Optional coverflow override: `/covers/homebrew/CaveStory_<loc>.img`
  (JPEG ≤186×100, ≤10 KiB).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
