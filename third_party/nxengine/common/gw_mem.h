/*
 * G&W memory helpers for NXEngine.
 *
 * Only AHB (newlib heap) supports realloc/free. RAM_EMU / DTCM / ITCM /
 * LUT8 bonus are bump pools — grow via alloc+copy, never realloc.
 */
#ifndef NXENGINE_GW_MEM_H
#define NXENGINE_GW_MEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call after lcd_setup_framebuffers(LCD_MODE_LUT8). */
void gw_mem_init(void);

int gw_is_ahb(const void *p);
void gw_free_ahb(void *p);

/* Prefer RAM_EMU → LUT8 bonus → DTCM → AHB. */
void *gw_alloc(size_t n);
void *gw_calloc(size_t count, size_t size);

/*
 * Grow a block. realloc ONLY when old is AHB. On failure returns NULL and
 * leaves the old pointer untouched.
 */
void *gw_grow(void *old, size_t old_bytes, size_t new_bytes);

#ifdef __cplusplus
}
#endif

#endif /* NXENGINE_GW_MEM_H */
