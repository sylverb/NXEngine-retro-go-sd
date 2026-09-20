
#ifndef _TILESET_H
#define _TILESET_H

#define TILE_W				16
#define TILE_H				16

namespace Tileset
{
	bool Init();
	void Close();
	
	bool Load(int new_tileset);
	void Reload();
	/* Drop current tileset so the next Load() re-runs palette_add (required
	 * after palette_reset — index remaps are otherwise stale). */
	void Invalidate();
	void draw_tile(int x, int y, int t);

#ifdef NXENGINE_GW
	/* XIP tilesets are read-only — store 16×16 screen-CLUT patches for
	 * destroyable crates / motion tiles instead of CopySpriteToTile COW. */
	void ClearPatches();
	void PatchFromSheet(int tileno, NXSurface *sheet, int srcx, int srcy);
#endif
	
	NXSurface *GetSurface();
};


#endif
