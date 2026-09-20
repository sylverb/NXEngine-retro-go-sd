
// manages the tileset
#include "graphics.h"
#include "tileset.h"
#include "tileset.fdh"
using namespace Graphics;

extern const char *tileset_names[];		// from stagedata.cpp
extern const char *stage_dir;			// from main

static NXSurface *tileset;
static int current_tileset = -1;

#ifdef NXENGINE_GW
#include "gw_mem.h"
#include <string.h>

#define SDL_GW_BOTTOMUP  0x10000000u

static uint8_t *tile_patch[256];

void Tileset::ClearPatches()
{
	for (int i = 0; i < 256; i++) {
		if (tile_patch[i]) {
			gw_free_ahb(tile_patch[i]);
			tile_patch[i] = NULL;
		}
	}
}

void Tileset::PatchFromSheet(int tileno, NXSurface *sheet, int srcx, int srcy)
{
	if (tileno < 0 || tileno >= 256 || !sheet)
		return;
	SDL_Surface *src = sheet->GetSDLSurface();
	if (!src || !src->pixels || !src->format)
		return;

	uint8_t *p = tile_patch[tileno];
	if (!p) {
		p = (uint8_t *)gw_alloc((size_t)TILE_W * (size_t)TILE_H);
		if (!p)
			return;
		tile_patch[tileno] = p;
	}

	/* Start clear (black / colorkey), then overlay opaque sprite pixels. */
	memset(p, 0, (size_t)TILE_W * (size_t)TILE_H);

	const uint8_t *remap = src->gw_index_remap;
	int use_key = (src->flags & SDL_SRCCOLORKEY) != 0;
	uint8_t key = (uint8_t)src->format->colorkey;
	int bottomup = (src->flags & SDL_GW_BOTTOMUP) != 0;

	for (int row = 0; row < TILE_H; row++) {
		int sy = srcy + row;
		if (sy < 0 || sy >= src->h)
			continue;
		if (bottomup)
			sy = src->h - 1 - sy;
		const uint8_t *line =
			(const uint8_t *)src->pixels + (size_t)sy * (size_t)src->pitch + (size_t)srcx;
		uint8_t *dst = p + row * TILE_W;
		for (int col = 0; col < TILE_W; col++) {
			if (srcx + col < 0 || srcx + col >= src->w)
				continue;
			uint8_t idx = line[col];
			uint8_t out = remap ? remap[idx] : idx;
			if (use_key && out == key)
				continue;
			dst[col] = out;
		}
	}
}
#endif

bool Tileset::Init()
{
	tileset = NULL;
	current_tileset = -1;
#ifdef NXENGINE_GW
	memset(tile_patch, 0, sizeof(tile_patch));
#endif
	return 0;
}

void Tileset::Close()
{
#ifdef NXENGINE_GW
	ClearPatches();
#endif
	delete tileset;
	tileset = NULL;
	current_tileset = -1;
}

void Tileset::Invalidate()
{
#ifdef NXENGINE_GW
	ClearPatches();
#endif
	delete tileset;
	tileset = NULL;
	current_tileset = -1;
}

/*
void c------------------------------() {}
*/

// load the given tileset into memory, replacing any other tileset.
bool Tileset::Load(int new_tileset)
{
char fname[MAXPATHLEN];

	if (new_tileset != current_tileset)
	{
		if (tileset)
		{
#ifdef NXENGINE_GW
			ClearPatches();
#endif
			delete tileset;
			current_tileset = -1;
		}
		
		sprintf(fname, "%s/Prt%s.pbm", stage_dir, tileset_names[new_tileset]);
		
#ifdef NXENGINE_GW
		/* Keep 8bpp XIP — DisplayFormat would allocate 256x240x2 (~120 KiB). */
		tileset = NXSurface::FromFile(fname, true, false);
#else
		// always use SDL_DisplayFormat on tilesets; they need to come out of 8-bit
		// so that we can replace the destroyable star tiles without them palletizing.
		tileset = NXSurface::FromFile(fname, true, true);
#endif
		if (!tileset)
		{
			return 1;
		}
		
		current_tileset = new_tileset;
	}
	
	return 0;
}

// draw the given tile from the current tileset to the screen
void Tileset::draw_tile(int x, int y, int t)
{
#ifdef NXENGINE_GW
	if (t >= 0 && t < 256 && tile_patch[t]) {
		static SDL_Surface wrap;
		static SDL_PixelFormat fmt;
		static int inited;
		if (!inited) {
			memset(&wrap, 0, sizeof(wrap));
			memset(&fmt, 0, sizeof(fmt));
			fmt.BitsPerPixel = 8;
			fmt.BytesPerPixel = 1;
			wrap.format = &fmt;
			wrap.w = TILE_W;
			wrap.h = TILE_H;
			wrap.pitch = TILE_W;
			inited = 1;
		}
		wrap.pixels = tile_patch[t];
		NXSurface nxs(&wrap, false);
		DrawSurface(&nxs, x, y);
		return;
	}
#endif
	// 16 tiles per row on all tilesheet
	int srcx = (t % 16) * TILE_W;
	int srcy = (t / 16) * TILE_H;
	
	DrawSurface(tileset, x, y, srcx, srcy, TILE_W, TILE_H);
}

void Tileset::Reload()
{
	if (current_tileset != -1)
	{
		int tileset = current_tileset;
		current_tileset = -1;
#ifdef NXENGINE_GW
		ClearPatches();
#endif
		Load(tileset);
	}
}

/*
void c------------------------------() {}
*/

NXSurface *Tileset::GetSurface()
{
	return tileset;
}
