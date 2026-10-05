/* Used-glyph CJK atlas (XIP / file). Packed by scripts/bake_cjk_atlas.py. */
#ifndef _CJKFONT_H
#define _CJKFONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Format "CJK1": magic + cell_w/h + count + sorted (cp, offset, advance) + 1bpp bitmaps. */
#define CJKFONT_MAGIC		0x314B4A43u	/* 'CJK1' LE */
#define CJKFONT_FILE		"cjkfont.dat"

bool cjkfont_init(void);
void cjkfont_close(void);
bool cjkfont_loaded(void);

/* Decode one UTF-8 codepoint; advances *text. Returns 0 on NUL/invalid. */
uint32_t utf8_next(const char **text);

/* Advance width in pixels for a Unicode codepoint (smalfont/CJK/space). */
int cjkfont_glyph_width(uint32_t cp);

/* Blit one glyph at (x,y) in screen coords (already * SCALE like text_draw).
 * color is 0x00RRGGBB. Returns advance width in pixels. */
int cjkfont_draw_glyph(int x, int y, uint32_t cp, uint32_t color, bool render);

int cjkfont_cell_height(void);

#ifdef __cplusplus
}
#endif

#endif
