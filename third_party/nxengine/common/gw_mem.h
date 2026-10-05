/*
 * G&W memory helpers for NXEngine.
 *
 * Two worlds:
 *   gw_alloc / gw_calloc     — bump pools (RAM_EMU → bonus → DTCM). No free.
 *                              Session data + Object/Caret freelist backing.
 *   gw_alloc_ahb / gw_free   — AHB newlib heap. Real malloc/free.
 *                              Spritesheet / tileset / backdrop wrappers only.
 *
 * Never put gameplay Objects on AHB (~90 KiB); never expect bump to reclaim.
 */
#ifndef NXENGINE_GW_MEM_H
#define NXENGINE_GW_MEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gw_mem_init(void);
void gw_mem_log(const char *tag);

int gw_is_ahb(const void *p);
void gw_free_ahb(void *p);

/* Bump only — never touches AHB. */
void *gw_alloc(size_t n);
void *gw_calloc(size_t count, size_t size);

/* AHB newlib only — freeable. */
void *gw_alloc_ahb(size_t n);
void *gw_calloc_ahb(size_t count, size_t size);

void *gw_grow(void *old, size_t old_bytes, size_t new_bytes);

#ifdef __cplusplus
}
#endif

#endif /* NXENGINE_GW_MEM_H */
