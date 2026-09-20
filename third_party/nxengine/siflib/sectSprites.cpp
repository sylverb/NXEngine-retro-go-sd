
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/DBuffer.h"
#include "../common/bufio.h"

#include "sectSprites.h"
#include "sectSprites.fdh"

#ifdef NXENGINE_GW
#include "gw_malloc.h"
#endif


int SIFSpritesSect::GetSpriteCount(const uint8_t *data, int datalen)
{
	const uint8_t *data_end = data + (datalen - 1);
	return read_U16(&data, data_end);
}


bool SIFSpritesSect::Decode(const uint8_t *data, int datalen, \
						  SIFSprite *sprites, int *nsprites_out, int maxsprites)
{
const uint8_t *data_end = data + (datalen - 1);
int i, f, nsprites;
	
	nsprites = read_U16(&data, data_end);
	if (nsprites_out) *nsprites_out = nsprites;
	
	if (nsprites >= maxsprites)
	{
		staterr("SIFSpritesSect::Decode: too many sprites in file (nsprites=%d, maxsprites=%d)", nsprites, maxsprites);
		return 1;
	}
	
#ifdef NXENGINE_GW
	/* Two-pass: one contiguous RAM_EMU slab for all dirs (leave DTCM for fonts). */
	{
		const uint8_t *scan = data;
		size_t total_dirs = 0;
		for (i = 0; i < nsprites; i++) {
			int w, h, sheet, nframes, ndirs_file, ndirs_alloc, n;
			int bi;
			w = read_U8(&scan, data_end);
			h = read_U8(&scan, data_end);
			sheet = read_U8(&scan, data_end);
			nframes = read_U8(&scan, data_end);
			ndirs_file = read_U8(&scan, data_end);
			(void)w; (void)h; (void)sheet;
			if (ndirs_file > SIF_MAX_DIRS) {
				staterr("SIFSpritesSect::Decode: SIF_MAX_DIRS exceeded (pass1)");
				return 1;
			}
			LoadRect(&sprites[0].bbox, &scan, data_end); /* scratch; rewritten below */
			LoadRect(&sprites[0].solidbox, &scan, data_end);
			LoadPoint(&sprites[0].spawn_point, &scan, data_end);
			LoadPointList(&sprites[0].block_l, &scan, data_end);
			LoadPointList(&sprites[0].block_r, &scan, data_end);
			LoadPointList(&sprites[0].block_u, &scan, data_end);
			LoadPointList(&sprites[0].block_d, &scan, data_end);
			ndirs_alloc = ndirs_file < 2 ? 2 : ndirs_file;
			n = nframes * ndirs_alloc;
			total_dirs += (size_t)n;
			for (f = 0; f < nframes; f++) {
				SIFFrame tmp;
				if (LoadFrame(&tmp, ndirs_file, &scan, data_end))
					return 1;
			}
			(void)bi;
		}

		SIFDir *dir_pool = NULL;
		if (total_dirs) {
			dir_pool = (SIFDir *)ram_malloc(total_dirs * sizeof(SIFDir));
			if (!dir_pool)
				dir_pool = (SIFDir *)dtc_malloc(total_dirs * sizeof(SIFDir));
		}
		if (!dir_pool && total_dirs) {
			staterr("SIFSpritesSect::Decode: OOM dir pool (%u dirs) ram=%u dtc=%u",
			        (unsigned)total_dirs,
			        (unsigned)ram_get_free_size(),
			        (unsigned)dtc_get_free_size());
			return 1;
		}
		if (dir_pool)
			memset(dir_pool, 0, total_dirs * sizeof(SIFDir));
		stat("SIFSpritesSect: building dir pool %u dirs (%u bytes)",
		     (unsigned)total_dirs,
		     (unsigned)(total_dirs * sizeof(SIFDir)));

		SIFDir *dir_cursor = dir_pool;
		for (i = 0; i < nsprites; i++) {
			int ndirs_file, ndirs_alloc, n;

			if (data > data_end) {
				staterr("SIFSpritesSect::Decode: section corrupt: overran end of data");
				return 1;
			}

			sprites[i].w = read_U8(&data, data_end);
			sprites[i].h = read_U8(&data, data_end);
			sprites[i].spritesheet = read_U8(&data, data_end);
			sprites[i].nframes = read_U8(&data, data_end);
			sprites[i].ndirs = read_U8(&data, data_end);
			ndirs_file = sprites[i].ndirs;

			if (ndirs_file > SIF_MAX_DIRS) {
				staterr("SIFSpritesSect::Decode: SIF_MAX_DIRS exceeded on sprite %d (ndirs=%d)", i, ndirs_file);
				return 1;
			}

			LoadRect(&sprites[i].bbox, &data, data_end);
			LoadRect(&sprites[i].solidbox, &data, data_end);
			LoadPoint(&sprites[i].spawn_point, &data, data_end);
			LoadPointList(&sprites[i].block_l, &data, data_end);
			LoadPointList(&sprites[i].block_r, &data, data_end);
			LoadPointList(&sprites[i].block_u, &data, data_end);
			LoadPointList(&sprites[i].block_d, &data, data_end);

			ndirs_alloc = ndirs_file < 2 ? 2 : ndirs_file;
			n = sprites[i].nframes * ndirs_alloc;
			sprites[i].dirs = dir_cursor;
			dir_cursor += n;
			sprites[i].ndirs = ndirs_alloc;

			for (f = 0; f < sprites[i].nframes; f++) {
				SIFFrame tmp;
				memset(&tmp, 0, sizeof(tmp));
				if (LoadFrame(&tmp, ndirs_file, &data, data_end))
					return 1;
				for (int d = 0; d < ndirs_alloc; d++) {
					int src = (d < ndirs_file) ? d : 0;
					sprites[i].dirs[f * ndirs_alloc + d] = tmp.dir[src];
				}
			}
		}
	}
#else
	for(i=0;i<nsprites;i++)
	{
		if (data > data_end)
		{
			staterr("SIFSpritesSect::Decode: section corrupt: overran end of data");
			return 1;
		}
		
		// read sprite-level fields
		sprites[i].w = read_U8(&data, data_end);
		sprites[i].h = read_U8(&data, data_end);
		sprites[i].spritesheet = read_U8(&data, data_end);
		
		sprites[i].nframes = read_U8(&data, data_end);
		sprites[i].ndirs = read_U8(&data, data_end);
		
		if (sprites[i].ndirs > SIF_MAX_DIRS)
		{
			staterr("SIFSpritesSect::Decode: SIF_MAX_DIRS exceeded on sprite %d (ndirs=%d)", i, sprites[i].ndirs);
			return 1;
		}
		
		LoadRect(&sprites[i].bbox, &data, data_end);
		LoadRect(&sprites[i].solidbox, &data, data_end);
		
		LoadPoint(&sprites[i].spawn_point, &data, data_end);
		
		LoadPointList(&sprites[i].block_l, &data, data_end);
		LoadPointList(&sprites[i].block_r, &data, data_end);
		LoadPointList(&sprites[i].block_u, &data, data_end);
		LoadPointList(&sprites[i].block_d, &data, data_end);
		
		// malloc enough space to hold the specified number
		// of apple fritters, i mean, frames.
		sprites[i].frame = (SIFFrame *)malloc(sizeof(SIFFrame) * sprites[i].nframes);
		
		// then load all frames
		for(f=0;f<sprites[i].nframes;f++)
		{
			if (LoadFrame(&sprites[i].frame[f], sprites[i].ndirs, &data, data_end))
				return 1;
		}
	}
#endif

	return 0;
}

bool SIFSpritesSect::LoadFrame(SIFFrame *frame, int ndirs, \
					const uint8_t **data, const uint8_t *data_end)
{
	// sets defaults for un-specified/default fields
	memset(frame, 0, sizeof(SIFFrame));
	
	for(int d=0;d<ndirs;d++)
	{
		SIFDir *dir = &frame->dir[d];
		LoadPoint(&dir->sheet_offset, data, data_end);
		
		int t;
		for(;;)
		{
			t = read_U8(data, data_end);
			if (t == S_DIR_END) break;
			
			switch(t)
			{
				case S_DIR_DRAW_POINT: LoadPoint(&dir->drawpoint, data, data_end); break;
				case S_DIR_ACTION_POINT: LoadPoint(&dir->actionpoint, data, data_end); break;
				case S_DIR_ACTION_POINT_2: LoadPoint(&dir->actionpoint2, data, data_end); break;
				
				case S_DIR_PF_BBOX:
					LoadRect(&dir->pf_bbox, data, data_end);
				break;
				
				default:
					stat("SIFSpriteSect::LoadFrame: encountered unknown optional field type %d", t);
				return 1;
			}
		}
	}
	
	return 0;
}

/*
void c------------------------------() {}
*/

void SIFSpritesSect::LoadRect(SIFRect *rect, const uint8_t **data, const uint8_t *data_end)
{
	rect->x1 = (int16_t)read_U16(data, data_end);
	rect->y1 = (int16_t)read_U16(data, data_end);
	rect->x2 = (int16_t)read_U16(data, data_end);
	rect->y2 = (int16_t)read_U16(data, data_end);
}

void SIFSpritesSect::LoadPoint(SIFPoint *pt, const uint8_t **data, const uint8_t *data_end)
{
	pt->x = (int16_t)read_U16(data, data_end);
	pt->y = (int16_t)read_U16(data, data_end);
}

void SIFSpritesSect::LoadPointList(SIFPointList *lst, const uint8_t **data, const uint8_t *data_end)
{
	lst->count = read_U8(data, data_end);
	if (lst->count > SIF_MAX_BLOCK_POINTS)
	{
		staterr("SIFSpritesSect::LoadPointList: too many block points (%d, max=%d)", lst->count, SIF_MAX_BLOCK_POINTS);
		return;
	}
	
	for(int i=0;i<lst->count;i++)
	{
		lst->point[i].x = (int16_t)read_U16(data, data_end);
		lst->point[i].y = (int16_t)read_U16(data, data_end);
	}
}

/*
void c------------------------------() {}
*/

uint8_t *SIFSpritesSect::Encode(SIFSprite *sprites, int nsprites, int *datalen_out)
{
#ifdef NXENGINE_GW
	(void)sprites; (void)nsprites;
	if (datalen_out) *datalen_out = 0;
	return NULL;
#else
DBuffer buf;
int i, f;

	buf.Append16(nsprites);
	
	for(i=0;i<nsprites;i++)
	{
		buf.Append8(sprites[i].w);
		buf.Append8(sprites[i].h);
		buf.Append8(sprites[i].spritesheet);
		
		buf.Append8(sprites[i].nframes);
		buf.Append8(sprites[i].ndirs);
		
		SaveRect(&sprites[i].bbox, &buf);
		SaveRect(&sprites[i].solidbox, &buf);
		
		SavePoint(&sprites[i].spawn_point, &buf);
		
		SavePointList(&sprites[i].block_l, &buf);
		SavePointList(&sprites[i].block_r, &buf);
		SavePointList(&sprites[i].block_u, &buf);
		SavePointList(&sprites[i].block_d, &buf);
		
		for(f=0;f<sprites[i].nframes;f++)
		{
			SaveFrame(&sprites[i].frame[f], sprites[i].ndirs, &buf);
		}
	}
	
	if (datalen_out) *datalen_out = buf.Length();
	return buf.TakeData();
#endif
}


void SIFSpritesSect::SaveFrame(SIFFrame *frame, int ndirs, DBuffer *out)
{
	for(int d=0;d<ndirs;d++)
	{
		SIFDir *dir = &frame->dir[d];
		
		SavePoint(&dir->sheet_offset, out);
		SaveOptionalPoint(S_DIR_DRAW_POINT, &dir->drawpoint, out);
		SaveOptionalPoint(S_DIR_ACTION_POINT, &dir->actionpoint, out);
		SaveOptionalPoint(S_DIR_ACTION_POINT_2, &dir->actionpoint2, out);
		SaveOptionalRect(S_DIR_PF_BBOX, &dir->pf_bbox, out);
		
		out->Append8(S_DIR_END);
	}
}

/*
void c------------------------------() {}
*/

void SIFSpritesSect::SaveRect(SIFRect *rect, DBuffer *out)
{
	out->Append16((uint16_t)rect->x1);
	out->Append16((uint16_t)rect->y1);
	out->Append16((uint16_t)rect->x2);
	out->Append16((uint16_t)rect->y2);
}

void SIFSpritesSect::SavePoint(SIFPoint *pt, DBuffer *out)
{
	out->Append16((uint16_t)pt->x);
	out->Append16((uint16_t)pt->y);
}

void SIFSpritesSect::SavePointList(SIFPointList *lst, DBuffer *out)
{
	out->Append8(lst->count);
	for(int i=0;i<lst->count;i++)
	{
		out->Append16((uint16_t)lst->point[i].x);
		out->Append16((uint16_t)lst->point[i].y);
	}
}

void SIFSpritesSect::SaveOptionalPoint(int type, SIFPoint *pt, DBuffer *out)
{
	if (!pt->equ(0, 0))
	{
		out->Append8(type);
		SavePoint(pt, out);
	}
}

void SIFSpritesSect::SaveOptionalRect(int type, SIFRect *rect, DBuffer *out)
{
	if (!rect->equ(0, 0, 0, 0))
	{
		out->Append8(type);
		SaveRect(rect, out);
	}
}






