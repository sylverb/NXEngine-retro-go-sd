# Cave Story (NXEngine) — GWHB for Game & Watch Retro-Go SD

Port of [EXL/NXEngine](https://github.com/EXL/NXEngine) (Cave Story engine) as a
**homebrew** binary for
[game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd).

## Status

`make` produces a packable **`CaveStory.bin`**. Host build (`make host`) is
playable. Game data is a single **`cavestory.nxpk`** (~6 MiB) flash-cached once
at boot; assets are XIP lookups (no per-file flash writes).

## Build

```bash
make pack-assets            # → CaveStory/cavestory.nxpk + sd_content copy
make                        # → CaveStory.bin (G&W)
make host                   # → CaveStory_host (desktop NXEngine)
./CaveStory_host            # play using ./CaveStory/ (validates .nxpk at start)
```

`pack-assets` converts 4bpp sheets to 8bpp, decrypts TSC offline, and packs
everything into `cavestory.nxpk`. Optional `make prepare-assets` still writes
loose `*.u8.bmp` siblings for host debugging.

Desktop needs SDL 1.2 (`brew install sdl12-compat`). First launch extracts
music/SFX from `Doukutsu.exe` into `CaveStory/` (one-time).

## SD layout

```
/homebrews/CaveStory.bin
/homebrews/cavestory.nxpk              ← required (make pack-assets)
/data/homebrew/cavestory_settings.dat  ← options (created at runtime)
/data/homebrew/cavestory_profile.dat   ← save slot 0 (+ profile2.dat, …)
```

## Requirements

- `arm-none-eabi-gcc` (hard-float `fpv5-d16`)
- Python 3 (+ Pillow if you use `prepare-assets`)
- Freeware Cave Story 1.0.0.6 data (not redistributed here)

## License

- This port / glue: see `LICENSE`
- NXEngine: GPL-3.0 (see `third_party/nxengine/LICENSE`)
- Cave Story assets: © Studio Pixel (provide your own copy)
