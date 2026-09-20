#include "gw_flash_assets.h"
#include "gw_nx_config.h"

#include <stdio.h>
#include <string.h>

#ifndef HOST_BUILD
#include "gw_flash_alloc.h"
#include "odroid_overlay.h"
#include "odroid_system.h"
void wdog_refresh(void);
#else
static uint8_t s_host_blob[256 * 1024];
static char s_host_key[128];
static uint32_t s_host_size;
#endif

char *gw_flash_asset_key(char *out, size_t out_sz, const char *suffix)
{
    if (!out || out_sz == 0)
        return out;
    snprintf(out, out_sz, "%s%s", GW_NX_FLASH_KEY_PFX, suffix ? suffix : "");
    return out;
}

const uint8_t *gw_flash_asset_lookup(const char *key, uint32_t *size_out)
{
#ifndef HOST_BUILD
    return lookup_data_in_flash(key, size_out);
#else
    if (key && s_host_size && strcmp(key, s_host_key) == 0) {
        if (size_out)
            *size_out = s_host_size;
        return s_host_blob;
    }
    if (size_out)
        *size_out = 0;
    return NULL;
#endif
}

const uint8_t *gw_flash_asset_store(const char *key, const uint8_t *data, uint32_t size)
{
#ifndef HOST_BUILD
    return store_data_in_flash(key, data, size);
#else
    if (!key || !data || size == 0 || size > sizeof(s_host_blob))
        return NULL;
    memcpy(s_host_blob, data, size);
    strncpy(s_host_key, key, sizeof(s_host_key) - 1);
    s_host_key[sizeof(s_host_key) - 1] = '\0';
    s_host_size = size;
    return s_host_blob;
#endif
}

const uint8_t *gw_flash_asset_cache_file(const char *sd_path, uint32_t *size_out)
{
#ifndef HOST_BUILD
    uint32_t sz = 0;
    uint8_t *p = odroid_overlay_cache_file_in_flash(sd_path, &sz, false);
    if (size_out)
        *size_out = sz;
    return p;
#else
    (void)sd_path;
    if (size_out)
        *size_out = 0;
    return NULL;
#endif
}

const uint8_t *gw_flash_asset_store_streamed(const char *key, uint32_t total_size,
                                             gw_flash_fill_cb_t fill_cb, void *user)
{
    if (!key || !fill_cb || total_size == 0)
        return NULL;

#ifndef HOST_BUILD
    /* OSPI_Program asserts buffer_size <= 256 — never append more. */
    enum { OSPI_PAGE = 256 };
    flash_stream_t st;
    memset(&st, 0, sizeof(st));
    if (!store_data_begin(&st, key, total_size))
        return NULL;

    /* AXI/AHB buffer — stack is DTCM and may be unusable for OSPI DMA. */
    static uint8_t chunk[4096];
    uint32_t remaining = total_size;
    while (remaining > 0) {
        wdog_refresh();
        uint32_t want = remaining < sizeof(chunk) ? remaining : (uint32_t)sizeof(chunk);
        int n = fill_cb(user, chunk, want);
        if (n <= 0) {
            store_data_abort(&st);
            return NULL;
        }
        if ((uint32_t)n > want)
            n = (int)want;
        uint32_t off = 0;
        while (off < (uint32_t)n) {
            uint32_t piece = (uint32_t)n - off;
            if (piece > OSPI_PAGE)
                piece = OSPI_PAGE;
            if (!store_data_append(&st, chunk + off, piece)) {
                store_data_abort(&st);
                return NULL;
            }
            off += piece;
        }
        remaining -= (uint32_t)n;
    }
    return store_data_finish(&st);
#else
    if (total_size > sizeof(s_host_blob))
        return NULL;
    uint32_t done = 0;
    while (done < total_size) {
        uint32_t want = total_size - done;
        if (want > 4096)
            want = 4096;
        int n = fill_cb(user, s_host_blob + done, want);
        if (n <= 0)
            return NULL;
        done += (uint32_t)n;
    }
    strncpy(s_host_key, key, sizeof(s_host_key) - 1);
    s_host_key[sizeof(s_host_key) - 1] = '\0';
    s_host_size = total_size;
    return s_host_blob;
#endif
}
