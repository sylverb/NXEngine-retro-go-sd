/*
 * Cave Story (NXEngine) — G&W Retro-Go SD port configuration.
 *
 * SD layout (multi-locale):
 *   /homebrews/CaveStory_<loc>.bin   — GWHB (display name "Cave Story XX")
 *   /homebrews/cavestory_<loc>.nxpk  — game data for that locale
 *   /data/homebrew/cavestory_*       — shared settings + save slots
 *
 * The homebrew derives the .nxpk path from ACTIVE_FILE's SD filename stem
 * (not the GWHB display name). Saves are shared across languages.
 */
#ifndef GW_NX_CONFIG_H
#define GW_NX_CONFIG_H

#include <stddef.h>

/* On-device paths (FatFs / retro-go). */
#define GW_NX_DATA_ROOT      "/homebrews"
/* Legacy single-pack name (fallback when ACTIVE_FILE stem has no locale). */
#define GW_NX_NXPK_PATH      GW_NX_DATA_ROOT "/cavestory.nxpk"
#define GW_NX_NXPK_PATH_MAX  96

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fill buf with the on-device NXPK path for this launch.
 * CaveStory_fr.bin → /homebrews/cavestory_fr.nxpk
 * CaveStory.bin (legacy) → /homebrews/cavestory.nxpk
 * Returns buf, or NULL if buflen is too small.
 */
char *gw_nx_resolve_nxpk_path(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* GW_NX_CONFIG_H */
