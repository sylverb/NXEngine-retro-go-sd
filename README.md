# Cave Story (NXEngine) — GWHB for Game & Watch Retro-Go SD

Port of [EXL/NXEngine](https://github.com/EXL/NXEngine) (Cave Story engine) as a
**homebrew** binary for
[game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd).

## Status

`make` produces **`CaveStory_en.bin`** (override with `LOCALE=fr`, …). Host
build (`make host`) is playable. Each language is a pair
**`CaveStory_<loc>.bin` + `cavestory_<loc>.nxpk`** (~6 MiB pack, flash-cached
once at boot; assets are XIP lookups). Several languages may sit side by side
under `/homebrews/`; saves are shared.

Catalogued locales: **en, de, es, fi, fr, it, nl, pt** (CP1252) plus **ja**
(Shift-JIS) and **ko** (UTF-8 via cavestory.one scrape). CJK packs bake a
used-glyph `cjkfont.dat` atlas from
[Fusion Pixel](https://github.com/TakWolf/fusion-pixel-font) 12px monospaced
(auto-downloaded at pack time — OFL, not vendored). Cyrillic / Turkish are
still skipped. Tagged releases ship one SD zip per language.

## Build

```bash
make                        # → CaveStory_en.bin (G&W)
make LOCALE=fr pack         # → CaveStory_fr.bin (same ELF, FR display name)
make pack-bins              # → CaveStory_<loc>.bin for every CI locale
make pack-assets            # → CaveStory/cavestory_en.nxpk + sd_content copy
make host                   # → CaveStory_host (desktop NXEngine)
./CaveStory_host            # play using ./CaveStory/ (validates .nxpk at start)
```

### Game data (`cavestory_<loc>.nxpk`)

CI downloads freeware packs from
[cavestory.one](https://www.cavestory.one/download/cave-story.php), extracts
embedded assets from `Doukutsu.exe`, builds `drum.pcm` / `sndcache.pcm`, then
packs one `.nxpk` per locale. Locally:

```bash
make ci-assets              # EN only
make ci-assets-all          # every locale in scripts/cavestory_locales.py
make pack-bins              # matching CaveStory_<loc>.bin for each
# or step by step:
python3 scripts/prepare_cavestory_tree.py   # needs network (or --archive path)
make host && ./CaveStory_host --ci-prepare
make pack-assets LOCALE=en
```

#### Other languages

Fan translations are usually a `data/` overlay (scripts + some images). All
locales share the **English** `Doukutsu.exe` extract for `org/`/`pxt/`/`endpic/`
(JP freeware uses different ORG offsets). **ja** overlays Japanese `data/` and
reads stage captions from JP Doukutsu (Shift-JIS → UTF-8). **ko** adds JP
`data/` sprites plus Korean TSC from cavestory.one (`scripts/fetch_ko_tsc_overlay.py`)
because the published `cavestory_k.7z` is a Windows PatchProgram. Needs `p7zip`
for `.7z` locales, `unrar` (or macOS `bsdtar`) for Spanish `.rar`. CJK atlases
use Fusion Pixel (fetched into `scripts/.cache/fusion-pixel/` on first
`pack-assets` for `ja`/`ko`).

```bash
make list-locales              # en de … ja ko …
make ci-assets LOCALE=de       # one locale
make ci-assets LOCALE=ja       # Japanese (EN music + JP data + cjkfont)
make ci-assets LOCALE=ko       # Korean (EN music + JP data + web TSC)
make ci-assets-all             # every CI locale (includes ko)
make pack-assets-fr            # shortcut → cavestory_fr.nxpk
```

At runtime the GWHB stem selects the pack: `CaveStory_de.bin` opens
`/homebrews/cavestory_de.nxpk`. Copy both files onto the SD (release zips
already pair them).

`pack-assets` converts 4bpp sheets to 8bpp, decrypts TSC offline, and packs
everything into the locale `.nxpk`. Optional `make prepare-assets` still writes
loose `*.u8.bmp` siblings for host debugging.

Desktop needs SDL 1.2 (`brew install sdl12-compat`) for the interactive host
build; `--ci-prepare` only needs the linked binary (no audio device).

## SD layout

```
/homebrews/CaveStory_en.bin            ← GWHB (display "Cave Story EN")
/homebrews/cavestory_en.nxpk           ← EN game data
/homebrews/CaveStory_fr.bin            ← optional second language
/homebrews/cavestory_fr.nxpk
/data/homebrew/cavestory_settings.dat  ← options (shared across languages)
/data/homebrew/cavestory_profile.dat   ← save slot 0 (+ profile2.dat, …)
```

Optional cover override: `/covers/homebrew/CaveStory_en.img` (JPEG ≤186×100,
≤10 KiB; stem matches the `.bin`). An embedded title-screen cover is packed
into the GWHB when present under `src/assets/cover.png`.

## Flash cache / hardfaults

`cavestory_*.nxpk` is mapped through the firmware circular **flash cache**. On a
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
