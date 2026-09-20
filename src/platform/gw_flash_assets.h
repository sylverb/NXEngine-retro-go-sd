/*
 * Thin helpers over the firmware derived-blob flash cache.
 *
 * Prefer these over calling store_data_* directly so keys stay namespaced
 * and host builds can stub the same API.
 */
#ifndef GW_FLASH_ASSETS_H
#define GW_FLASH_ASSETS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Probe cache. Returns memory-mapped pointer or NULL. */
const uint8_t *gw_flash_asset_lookup(const char *key, uint32_t *size_out);

/* Store a whole blob already in RAM (or return existing cache hit). */
const uint8_t *gw_flash_asset_store(const char *key, const uint8_t *data,
                                    uint32_t size);

/*
 * Ensure `sd_path` is resident in flash (raw file cache).
 * On hit/miss success returns XIP pointer; NULL on failure.
 */
const uint8_t *gw_flash_asset_cache_file(const char *sd_path, uint32_t *size_out);

/*
 * Stream a derived blob into flash without a full-size RAM buffer.
 * Returns mapped pointer, or NULL.
 *
 * `fill_cb` is called repeatedly until it returns 0 bytes written (done)
 * or negative (abort). Typical chunk size: a few KiB.
 */
typedef int (*gw_flash_fill_cb_t)(void *user, uint8_t *buf, uint32_t buf_cap);

const uint8_t *gw_flash_asset_store_streamed(const char *key, uint32_t total_size,
                                             gw_flash_fill_cb_t fill_cb, void *user);

/* Build a namespaced key into `out` (NUL-terminated). Returns out. */
char *gw_flash_asset_key(char *out, size_t out_sz, const char *suffix);

#ifdef __cplusplus
}
#endif

#endif /* GW_FLASH_ASSETS_H */
