
#include "../nx.h"
#include "palette.h"
#include "palette.fdh"
#ifdef NXENGINE_GW
#include "gw_malloc.h"
#include "gw_mem.h"
#include <string.h>
#endif

#define MAX_COLORS		256
static SDL_Color screenpal[MAX_COLORS];
int ncolors = -1;

// clear out all palette entries
void palette_reset(void)
{
	ncolors = 0;
#ifdef NXENGINE_GW
	memset(screenpal, 0, sizeof(screenpal));
#endif
}

// given a paletted surface add it's colors in to the screen colormap
// then return a surface with the color indexes remapped ready to
// be displayed on the screen. insfc is either freed, or reused to
// create the returned surface.
SDL_Surface *palette_add(SDL_Surface *sfc)
{
SDL_Palette *pal = sfc->format->palette;
int remap[MAX_COLORS];
int i;

	if (sfc->format->BitsPerPixel > 8)
	{
		staterr("palette_add: input surface is > 8bpp");
		return NULL;
	}
	
#ifdef NXENGINE_GW
	/* BMP palettes are often 256 slots with ~16 real colors; registering
	 * unused slots fills the CLUT with junk → nearest-match garbage and
	 * wrong collision-looking scenery after TRA. Only map used indices. */
	uint8_t used[MAX_COLORS];
	memset(used, 0, sizeof(used));
	used[0] = 1; /* black / colorkey */
	if (sfc->pixels && sfc->w > 0 && sfc->h > 0) {
		const int bottomup = (sfc->flags & 0x10000000u) != 0; /* SDL_GW_BOTTOMUP */
		for (int y = 0; y < sfc->h; y++) {
			int sy = bottomup ? (sfc->h - 1 - y) : y;
			const uint8_t *row =
				(const uint8_t *)sfc->pixels + (size_t)sy * (size_t)sfc->pitch;
			for (int x = 0; x < sfc->w; x++)
				used[row[x]] = 1;
		}
	} else {
		/* No pixels yet — fall back to full palette (should be rare). */
		for (i = 0; i < pal->ncolors && i < MAX_COLORS; i++)
			used[i] = 1;
	}

	int n_used = 0;
	for (i = 0; i < MAX_COLORS; i++)
		if (used[i]) n_used++;
	stat("palette_add: %d used colors (pal slots %d), screen has %d",
	     n_used, pal->ncolors, ncolors);

	for (i = 0; i < MAX_COLORS; i++) {
		if (!used[i] || i >= pal->ncolors) {
			remap[i] = 0;
			continue;
		}
		remap[i] = palette_alloc(pal->colors[i].r, pal->colors[i].g, pal->colors[i].b);
		if (remap[i] == -1)
			remap[i] = 0;
	}
#else
	stat("palette_add: adding %d colors to screen palette...", pal->ncolors);
	for(i=0;i<pal->ncolors;i++)
	{
		remap[i] = palette_alloc(pal->colors[i].r, pal->colors[i].g, pal->colors[i].b);
		if (remap[i] == -1)
			return sfc;
	}
#endif
	
	SDL_SetColors(screen->GetSDLSurface(), screenpal, 0, ncolors);

#ifdef NXENGINE_GW
	/* Sheets stay XIP in flash — attach a 256-byte index remap for blit,
	 * do not rewrite pixel bytes. */
	{
		Uint8 *table = sfc->gw_index_remap;
		if (!table) {
			table = (Uint8 *)gw_calloc(256, 1);
			if (!table)
				table = (Uint8 *)calloc(256, 1);
			sfc->gw_index_remap = table;
		}
		if (table) {
			for (i = 0; i < MAX_COLORS; i++)
				table[i] = (Uint8)remap[i];
			if (sfc->flags & SDL_SRCCOLORKEY) {
				Uint32 ck = sfc->format->colorkey;
				if (ck < MAX_COLORS)
					sfc->format->colorkey = table[ck];
			}
		}
	}
#else
	(void)remap;
#endif
	return sfc;
}


// add the given color to the screen palette and return it's index.
int palette_alloc(uint8_t r, uint8_t g, uint8_t b)
{
int i;

	for(i=0;i<ncolors;i++)
	{
		if (screenpal[i].r == r && \
			screenpal[i].g == g && \
			screenpal[i].b == b)
		{
			return i;
		}
	}
	
#ifdef NXENGINE_GW
	/* Slots 240..255 reserved for font glyphs (SDL_GW_AllocColor). */
	const int limit = 240;
#else
	const int limit = MAX_COLORS;
#endif
	if (ncolors >= limit)
	{
#ifdef NXENGINE_GW
		int best = 0, bestd = 1 << 30;
		for (i = 0; i < ncolors; i++) {
			int dr = (int)screenpal[i].r - r;
			int dg = (int)screenpal[i].g - g;
			int db = (int)screenpal[i].b - b;
			int d = dr * dr + dg * dg + db * db;
			if (d < bestd) {
				bestd = d;
				best = i;
			}
		}
		return best;
#else
		staterr("palette_alloc: out of color space!");
		return -1;
#endif
	}
	
	screenpal[ncolors].r = r;
	screenpal[ncolors].g = g;
	screenpal[ncolors].b = b;
	return ncolors++;
}
