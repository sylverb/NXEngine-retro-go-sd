# Cave Story (NXEngine) — GWHB for Game & Watch Retro-Go SD

Port of [EXL/NXEngine](https://github.com/EXL/NXEngine) (Cave Story engine) as a
**homebrew** binary for
[game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd).

## Status

`make` produces a packable **`CaveStory.bin`**. Host build (`make host`) is
playable. Game data is a single **`cavestory.nxpk`** (~6 MiB) flash-cached once
at boot; assets are XIP lookups (no per-file flash writes).

In-game text language comes from the **`.nxpk` pack** you install. Catalogued
locales today: **en, de, es, fi, fr, it, nl, pt** (CP1252 / Latin-1 fan packs).
CJK / Cyrillic / Turkish need engine font work and are not shipped yet.
Tagged releases ship one SD zip per language.

## Build

```bash
make                        # → CaveStory.bin (G&W)
make pack-assets            # → CaveStory/cavestory.nxpk + sd_content copy
make host                   # → CaveStory_host (desktop NXEngine)
./CaveStory_host            # play using ./CaveStory/ (validates .nxpk at start)
```

### Game data (`cavestory.nxpk`)

CI downloads freeware packs from
[cavestory.one](https://www.cavestory.one/download/cave-story.php), extracts
embedded assets from `Doukutsu.exe`, builds `drum.pcm` / `sndcache.pcm`, then
packs one `.nxpk` per locale. Locally:

```bash
make ci-assets              # EN only
make ci-assets-all          # every locale in scripts/cavestory_locales.py
# or step by step:
python3 scripts/prepare_cavestory_tree.py   # needs network (or --archive path)
make host && ./CaveStory_host --ci-prepare
make pack-assets
```

#### Other languages

Fan translations are usually a `data/` overlay (scripts + some images). Music/SFX
still come from the English `Doukutsu.exe` extract. Needs `p7zip` for `.7z`
locales and `unrar` (or macOS `bsdtar`) for Spanish `.rar`.

```bash
make list-locales              # en de es fi fr it nl pt
make ci-assets LOCALE=de       # one locale
make ci-assets-all             # every catalogued locale (CI)
make pack-assets-fr            # shortcut → cavestory_fr.nxpk
```

On the SD card the homebrew always opens `/homebrews/cavestory.nxpk`. Release
zips already use that name (`CaveStory-<tag>-de.zip`, etc.). From a local pack:

```bash
cp sd_content/homebrews/cavestory_de.nxpk /path/to/sd/homebrews/cavestory.nxpk
```

`pack-assets` converts 4bpp sheets to 8bpp, decrypts TSC offline, and packs
everything into the locale `.nxpk`. Optional `make prepare-assets` still writes
loose `*.u8.bmp` siblings for host debugging.

Desktop needs SDL 1.2 (`brew install sdl12-compat`) for the interactive host
build; `--ci-prepare` only needs the linked binary (no audio device).

## SD layout

```
/homebrews/CaveStory.bin
/homebrews/cavestory.nxpk              ← required (make pack-assets)
/data/homebrew/cavestory_settings.dat  ← options (created at runtime)
/data/homebrew/cavestory_profile.dat   ← save slot 0 (+ profile2.dat, …)
```

Optional cover override: `/covers/homebrew/CaveStory.img` (JPEG ≤186×100,
≤10 KiB). An embedded title-screen cover is packed into the GWHB when present
under `src/assets/cover.png`.

## Flash cache / hardfaults

`cavestory.nxpk` is mapped through the firmware circular **flash cache**. On a
large EXTFLASH (64–256 MiB) the pack can land anywhere in the OSPI window
(`0x90000000` + offset), not only near the start.

Pointers into that pack must **never** be passed to `free()`. Older builds used
a fixed “XIP = below 16/32 MiB” check; when the cache placed the pack higher,
TSC/org teardown freed flash addresses, corrupted the AHB heap, and hardfaulted
inside firmware `printf` (`memchr` / `_svfprintf_r`). Clearing Retro-Go settings
(or the flash cache) rewrote the pack at a low address and hid the bug.

Current builds use `gw_pack_ptr_in_pack()` against the mapped NXPK range, so
flash size and cache position no longer matter. If you still see a boot hang on
“bad nxpk”, clear the flash cache once so a stale entry is rewritten.

## Requirements

- `arm-none-eabi-gcc` (hard-float `fpv5-d16`)
- Python 3 (+ Pillow if you use `prepare-assets`)
- Freeware Cave Story 1.0.0.6 data (not redistributed here)
- Firmware ABI matching `SDK_VERSION` in this repository

## License

- This port / glue: see `LICENSE`
- NXEngine: GPL-3.0 (see `third_party/nxengine/LICENSE`)
- Cave Story assets: © Studio Pixel (provide your own copy)
