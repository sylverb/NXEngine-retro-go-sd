#include "gw_mem.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gw_lcd.h"
#include "gw_malloc.h"

static uint8_t *s_bonus;
static size_t s_bonus_left;

void gw_mem_init(void)
{
	s_bonus = NULL;
	s_bonus_left = 0;
	if (lcd_get_mode() == LCD_MODE_LUT8)
		lcd_get_bonus_pool(&s_bonus, &s_bonus_left);
	printf("gw_mem: bonus=%u B @ %p (lut8=%d)\n",
	       (unsigned)s_bonus_left, (void *)s_bonus,
	       lcd_get_mode() == LCD_MODE_LUT8);
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
	void *p = ram_malloc(n);
	if (!p)
		p = bonus_malloc(n);
	if (!p)
		p = dtc_malloc(n);
	/* Only touch AHB when it still has real headroom — avoids _sbrk spam. */
	if (!p && ahb_get_free_size() >= (n + 64u))
		p = malloc(n);
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

void *gw_grow(void *old, size_t old_bytes, size_t new_bytes)
{
	if (old && gw_is_ahb(old)) {
		size_t room = ahb_get_free_size();
		/* Only try realloc when AHB still has headroom (avoids OOM spam). */
		if (room >= 2048 && (new_bytes <= old_bytes || room >= (new_bytes - old_bytes))) {
			void *p = realloc(old, new_bytes);
			if (p)
				return p;
		}
	}

	void *p = gw_alloc(new_bytes);
	if (!p)
		return NULL;
	if (old && old_bytes > 0)
		memcpy(p, old, old_bytes < new_bytes ? old_bytes : new_bytes);
	gw_free_ahb(old);
	return p;
}
