#include "../config.h"
#include <SDL/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../nx.h"
#include "cjkfont.h"

#ifdef NXENGINE_GW
#include "gw_pack.h"
#endif

#pragma pack(push, 1)
struct CjkHeader {
	uint32_t magic;
	uint8_t cell_w;
	uint8_t cell_h;
	uint16_t count;
};

struct CjkEntry {
	uint32_t cp;
	uint32_t offset;	/* file-absolute into glyph blob */
	uint8_t advance;
	uint8_t reserved[3];
};
#pragma pack(pop)

#define CJK_LRU_SIZE	48

struct CjkLruSlot {
	uint32_t cp;
	uint32_t color;
	SDL_Surface *sfc;
	uint8_t advance;
	uint8_t used;
};

static const uint8_t *s_blob;
static uint32_t s_blob_size;
static uint8_t *s_owned;
static const CjkHeader *s_hdr;
static const CjkEntry *s_toc;
static bool s_loaded;
static CjkLruSlot s_lru[CJK_LRU_SIZE];
static int s_lru_clock;

uint32_t utf8_next(const char **text)
{
	const unsigned char *p = (const unsigned char *)(*text);
	if (!p || !p[0])
		return 0;

	uint32_t cp;
	int n;
	if (p[0] < 0x80) {
		cp = p[0];
		n = 1;
	} else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
		cp = ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
		n = 2;
	} else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
		cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
		n = 3;
	} else if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 &&
			   (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
		cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
			 ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
		n = 4;
	} else {
		*text = (const char *)(p + 1);
		return 0xFFFD;
	}
	*text = (const char *)(p + n);
	return cp;
}

static int find_entry(uint32_t cp)
{
	if (!s_loaded || !s_hdr)
		return -1;
	int lo = 0, hi = (int)s_hdr->count - 1;
	while (lo <= hi) {
		int mid = (lo + hi) >> 1;
		uint32_t v = s_toc[mid].cp;
		if (v == cp)
			return mid;
		if (v < cp)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

bool cjkfont_loaded(void)
{
	return s_loaded;
}

int cjkfont_cell_height(void)
{
	return s_loaded ? (int)s_hdr->cell_h : 0;
}

static bool map_blob(const uint8_t *data, uint32_t size, bool owned)
{
	if (!data || size < sizeof(CjkHeader))
		return false;
	const CjkHeader *hdr = (const CjkHeader *)data;
	if (hdr->magic != CJKFONT_MAGIC || hdr->cell_w == 0 || hdr->cell_h == 0)
		return false;
	uint32_t toc_bytes = (uint32_t)hdr->count * (uint32_t)sizeof(CjkEntry);
	if (size < sizeof(CjkHeader) + toc_bytes)
		return false;

	s_blob = data;
	s_blob_size = size;
	s_owned = owned ? (uint8_t *)data : NULL;
	s_hdr = hdr;
	s_toc = (const CjkEntry *)(data + sizeof(CjkHeader));
	s_loaded = true;
	memset(s_lru, 0, sizeof(s_lru));
	s_lru_clock = 0;
	stat("cjkfont: loaded %u glyphs cell=%ux%u (%u bytes)%s",
		 (unsigned)hdr->count, hdr->cell_w, hdr->cell_h, (unsigned)size,
		 owned ? "" : " XIP");
	return true;
}

bool cjkfont_init(void)
{
	cjkfont_close();

#ifdef NXENGINE_GW
	if (gw_pack_ready()) {
		uint32_t sz = 0;
		const uint8_t *blob = gw_pack_get(CJKFONT_FILE, &sz);
		if (blob && sz && map_blob(blob, sz, false))
			return true;
	}
#endif

	FILE *fp = fileopen(CJKFONT_FILE, "rb");
	if (!fp)
		return false;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (sz <= 0) {
		fclose(fp);
		return false;
	}
	uint8_t *buf = (uint8_t *)malloc((size_t)sz);
	if (!buf) {
		fclose(fp);
		return false;
	}
	if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
		free(buf);
		fclose(fp);
		return false;
	}
	fclose(fp);
	if (!map_blob(buf, (uint32_t)sz, true)) {
		free(buf);
		return false;
	}
	return true;
}

void cjkfont_close(void)
{
	for (int i = 0; i < CJK_LRU_SIZE; i++) {
		if (s_lru[i].sfc) {
			SDL_FreeSurface(s_lru[i].sfc);
			s_lru[i].sfc = NULL;
		}
	}
	memset(s_lru, 0, sizeof(s_lru));
	if (s_owned) {
		free(s_owned);
		s_owned = NULL;
	}
	s_blob = NULL;
	s_blob_size = 0;
	s_hdr = NULL;
	s_toc = NULL;
	s_loaded = false;
}

int cjkfont_glyph_width(uint32_t cp)
{
	if (cp == ' ')
		return 5;
	int idx = find_entry(cp);
	if (idx < 0)
		return (cp < 0x80) ? 5 : (s_loaded ? (int)s_hdr->cell_w : 5);
	return s_toc[idx].advance ? (int)s_toc[idx].advance : (int)s_hdr->cell_w;
}

static SDL_Surface *video_surface(void)
{
	if (screen) {
		SDL_Surface *s = screen->GetSDLSurface();
		if (s)
			return s;
	}
#ifndef NXENGINE_GW
	return SDL_GetVideoSurface();
#else
	return NULL;
#endif
}

static SDL_Surface *rasterize(uint32_t cp, uint32_t color, uint8_t *adv_out)
{
	int idx = find_entry(cp);
	if (idx < 0)
		return NULL;

	const CjkEntry *e = &s_toc[idx];
	uint8_t adv = e->advance ? e->advance : s_hdr->cell_w;
	if (adv_out)
		*adv_out = adv;

	for (int i = 0; i < CJK_LRU_SIZE; i++) {
		if (s_lru[i].sfc && s_lru[i].cp == cp && s_lru[i].color == color) {
			s_lru[i].used = 1;
			if (adv_out)
				*adv_out = s_lru[i].advance;
			return s_lru[i].sfc;
		}
	}

	int cw = s_hdr->cell_w;
	int ch = s_hdr->cell_h;
	uint32_t bits = (uint32_t)cw * (uint32_t)ch;
	uint32_t nbytes = (bits + 7u) >> 3;
	if ((uint64_t)e->offset + nbytes > s_blob_size)
		return NULL;
	const uint8_t *bitsrc = s_blob + e->offset;

	SDL_Surface *vid = video_surface();
	if (!vid)
		return NULL;
	SDL_PixelFormat *format = vid->format;

	SDL_Surface *letter = SDL_CreateRGBSurface(
		SDL_SRCCOLORKEY, cw + 1, ch + 1,
		format->BitsPerPixel,
		format->Rmask, format->Gmask, format->Bmask, format->Amask);
	if (!letter)
		return NULL;

	SDL_FillRect(letter, NULL, SDL_MapRGB(format, 0, 0, 0));

#ifdef NXENGINE_GW
	uint8_t dst_idx = (uint8_t)SDL_GW_AllocColor(
		(Uint8)((color >> 16) & 0xff),
		(Uint8)((color >> 8) & 0xff),
		(Uint8)(color & 0xff));
#else
	Uint32 fg = SDL_MapRGB(format,
		(Uint8)((color >> 16) & 0xff),
		(Uint8)((color >> 8) & 0xff),
		(Uint8)(color & 0xff));
#endif

	for (int y = 0; y < ch; y++) {
		for (int x = 0; x < cw; x++) {
			uint32_t bit = (uint32_t)y * (uint32_t)cw + (uint32_t)x;
			if (!(bitsrc[bit >> 3] & (0x80u >> (bit & 7))))
				continue;
#ifdef NXENGINE_GW
			if (letter->format->BytesPerPixel == 1)
				((uint8_t *)letter->pixels)[y * letter->pitch + x] = dst_idx;
#else
			if (letter->format->BytesPerPixel == 2) {
				((uint16_t *)((uint8_t *)letter->pixels + y * letter->pitch))[x] =
					(uint16_t)fg;
			} else if (letter->format->BytesPerPixel == 4) {
				((uint32_t *)((uint8_t *)letter->pixels + y * letter->pitch))[x] = fg;
			} else if (letter->format->BytesPerPixel == 1) {
				((uint8_t *)letter->pixels)[y * letter->pitch + x] = (uint8_t)fg;
			}
#endif
		}
	}

	SDL_SetColorKey(letter, SDL_SRCCOLORKEY, SDL_MapRGB(format, 0, 0, 0));

#ifndef NXENGINE_GW
	SDL_Surface *conv = SDL_DisplayFormat(letter);
	SDL_FreeSurface(letter);
	letter = conv;
	if (!letter)
		return NULL;
#endif

	for (int n = 0; n < CJK_LRU_SIZE; n++) {
		int i = (s_lru_clock + n) % CJK_LRU_SIZE;
		if (!s_lru[i].sfc || !s_lru[i].used) {
			if (s_lru[i].sfc)
				SDL_FreeSurface(s_lru[i].sfc);
			s_lru[i].cp = cp;
			s_lru[i].color = color;
			s_lru[i].sfc = letter;
			s_lru[i].advance = adv;
			s_lru[i].used = 1;
			s_lru_clock = (i + 1) % CJK_LRU_SIZE;
			return letter;
		}
		s_lru[i].used = 0;
	}
	int i = s_lru_clock;
	if (s_lru[i].sfc)
		SDL_FreeSurface(s_lru[i].sfc);
	s_lru[i].cp = cp;
	s_lru[i].color = color;
	s_lru[i].sfc = letter;
	s_lru[i].advance = adv;
	s_lru[i].used = 1;
	s_lru_clock = (i + 1) % CJK_LRU_SIZE;
	return letter;
}

int cjkfont_draw_glyph(int x, int y, uint32_t cp, uint32_t color, bool render)
{
	if (cp == ' ')
		return 5;

	int idx = find_entry(cp);
	if (idx < 0)
		return s_loaded ? (int)s_hdr->cell_w : 5;

	uint8_t adv = s_toc[idx].advance ? s_toc[idx].advance : s_hdr->cell_w;
	if (!render)
		return (int)adv;

	SDL_Surface *letter = rasterize(cp, color, &adv);
	if (!letter)
		return (int)adv;

	SDL_Surface *vid = video_surface();
	if (!vid)
		return (int)adv;

	SDL_Rect dst;
	dst.x = (Sint16)x;
	dst.y = (Sint16)y;
	SDL_BlitSurface(letter, NULL, vid, &dst);
	return (int)adv;
}
