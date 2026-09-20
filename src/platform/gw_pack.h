/*
 * Cave Story NXPK — single-file asset pack served from flash XIP (or host RAM).
 *
 * Format v1: magic "NXPK" + u16 ver + u16 count, then count ×
 *   { path[96], offset u32, size u32 }, then 4-aligned blobs.
 */
#ifndef GW_PACK_H
#define GW_PACK_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "gw_nx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GW_NXPK_PATH GW_NX_NXPK_PATH
#define GW_NXPK_PATH_LEN 96

/* Map a packed NXPK image already in memory (flash XIP or malloc). */
int gw_pack_init(const uint8_t *base, uint32_t size);

/* True after a successful gw_pack_init. */
bool gw_pack_ready(void);

/* Lookup by pack-relative path ("data/Head.tsc") or SD absolute under data root.
 * Returns a pointer into the mapped pack (do not free). */
const uint8_t *gw_pack_get(const char *relpath, uint32_t *size_out);

/* Read-only memfile over a pack blob (or any const buffer). */
FILE *gw_pack_mem_fopen(const uint8_t *data, uint32_t size);

/* True if FILE* is one of our memfile slots. */
bool gw_pack_is_memfile(FILE *fp);

/* Desktop: fopen+malloc a .nxpk then gw_pack_init. Returns 0 on success. */
int gw_pack_host_load(const char *path_hint);

#ifdef __cplusplus
}
#endif

#endif /* GW_PACK_H */
