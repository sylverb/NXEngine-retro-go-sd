#include "gw_mem.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gw_lcd.h"
#include "gw_malloc.h"

static uint8_t *s_bonus;
static size_t s_bonus_left;
static size_t s_bonus_total;

void gw_mem_init(void)
{
	s_bonus = NULL;
	s_bonus_left = 0;
	s_bonus_total = 0;
	if (lcd_get_mode() == LCD_MODE_LUT8) {
		lcd_get_bonus_pool(&s_bonus, &s_bonus_left);
		s_bonus_total = s_bonus_left;
	}
}

void gw_mem_log(const char *tag)
{
	printf("NXEngine: mem[%s] ram=%u ahb=%u dtc=%u itc=%u bonus=%u/%u\n",
	       tag ? tag : "?",
	       (unsigned)ram_get_free_size(),
	       (unsigned)ahb_get_free_size(),
	       (unsigned)dtc_get_free_size(),
	       (unsigned)itc_get_free_size(),
	       (unsigned)s_bonus_left,
	       (unsigned)s_bonus_total);
}

int gw_is_ahb(const void *p)
{
	uintptr_t a = (uintptr_t)p;
	return (a >= 0x30000000u && a < 0x30020000u);
}

void gw_free_ahb(void *p)
{
	if (p && gw_is_ahb(p))
		free(p);
}

static void *bonus_malloc(size_t n)
{
	n = (n + 7u) & ~7u;
	if (!s_bonus || n == 0 || n > s_bonus_left)
		return NULL;
	void *p = s_bonus;
	s_bonus += n;
	s_bonus_left -= n;
	return p;
}

void *gw_alloc(size_t n)
{
	/* Bump pools only — keep AHB free for FlushSheets metadata. */
	void *p = ram_malloc(n);
	if (!p)
		p = bonus_malloc(n);
	if (!p)
		p = dtc_malloc(n);
	return p;
}

void *gw_calloc(size_t count, size_t size)
{
	size_t n = count * size;
	void *p = gw_alloc(n ? n : 1);
	if (p)
		memset(p, 0, n);
	return p;
}

void *gw_alloc_ahb(size_t n)
{
	if (n == 0)
		n = 1;
	if (ahb_get_free_size() < (n + 256u))
		return NULL;
	return malloc(n);
}

void *gw_calloc_ahb(size_t count, size_t size)
{
	size_t n = count * size;
	void *p = gw_alloc_ahb(n ? n : 1);
	if (p)
		memset(p, 0, n);
	return p;
}

void *gw_grow(void *old, size_t old_bytes, size_t new_bytes)
{
	if (old && gw_is_ahb(old)) {
		void *p = realloc(old, new_bytes);
		if (p)
			return p;
		/* Fresh AHB block — never migrate script buffers onto bump. */
		p = gw_alloc_ahb(new_bytes);
		if (!p)
			return NULL;
		if (old_bytes > 0)
			memcpy(p, old, old_bytes < new_bytes ? old_bytes : new_bytes);
		free(old);
		return p;
	}

	/* Bump-backed grow: old block is stranded (no free on bump). */
	void *p = gw_alloc(new_bytes);
	if (!p)
		return NULL;
	if (old && old_bytes > 0)
		memcpy(p, old, old_bytes < new_bytes ? old_bytes : new_bytes);
	return p;
}
