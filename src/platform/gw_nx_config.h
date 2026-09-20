/*
 * Cave Story (NXEngine) — G&W Retro-Go SD port configuration.
 *
 * SD layout:
 *   /homebrews/CaveStory.bin              — this homebrew (GWHB)
 *   /homebrews/cavestory.nxpk             — all game data (make pack-assets)
 *   /data/homebrew/cavestory_settings.dat — options
 *   /data/homebrew/cavestory_profile*.dat — save slots
 *
 * Saves are FatFs files under /data/homebrew/; game assets come from NXPK XIP.
 */
#ifndef GW_NX_CONFIG_H
#define GW_NX_CONFIG_H

/* On-device paths (FatFs / retro-go). */
#define GW_NX_DATA_ROOT      "/homebrews"
#define GW_NX_NXPK_PATH      GW_NX_DATA_ROOT "/cavestory.nxpk"

#define GW_NX_SAVE_DIR       "/data/homebrew"
#define GW_NX_SAVE_PREFIX    "cavestory_"
#define GW_NX_SETTINGS_PATH  GW_NX_SAVE_DIR "/" GW_NX_SAVE_PREFIX "settings.dat"
#define GW_NX_PROFILE0_PATH  GW_NX_SAVE_DIR "/" GW_NX_SAVE_PREFIX "profile.dat"
#define GW_NX_PROFILE_FMT    GW_NX_SAVE_DIR "/" GW_NX_SAVE_PREFIX "profile%d.dat"

/* Flash-cache key prefix (legacy derived blobs — unused with NXPK). */
#define GW_NX_FLASH_KEY_PFX  "nx/"

/* Native game resolution (matches NXEngine _320X240). */
#define GW_NX_WIDTH   320
#define GW_NX_HEIGHT  240
#define GW_NX_FPS     50

/* Audio: firmware-supported rate; half-buffer = rate/fps ≤ 1077. */
#define GW_NX_SAMPLE_RATE  22050

#endif /* GW_NX_CONFIG_H */
