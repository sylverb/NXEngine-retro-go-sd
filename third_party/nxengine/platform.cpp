
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "platform.fdh"

#ifdef NXENGINE_GW
#include "gw_pack.h"
/* Prefix relative paths with the SD data root. */
#ifndef GW_NX_FS_ROOT
#define GW_NX_FS_ROOT "/homebrews"
#endif
static const char *gw_nx_root = GW_NX_FS_ROOT;

static int mode_is_readonly(const char *mode)
{
	if (!mode || !mode[0])
		return 0;
	/* "r", "rb", "r+b" still need FatFs when writing — only pure read. */
	if (mode[0] != 'r')
		return 0;
	for (const char *p = mode; *p; p++) {
		if (*p == '+')
			return 0;
		if (*p == 'w' || *p == 'a')
			return 0;
	}
	return 1;
}

FILE *fileopen(const char *fname, const char *mode)
{
	char path[256];
	if (!fname)
		return NULL;
	/* Skip "./" so FatFs gets clean paths under the data root. */
	while (fname[0] == '.' && fname[1] == '/')
		fname += 2;

	if (mode_is_readonly(mode) && gw_pack_ready()) {
		uint32_t sz = 0;
		const uint8_t *blob = gw_pack_get(fname, &sz);
		if (blob && sz > 0)
			return gw_pack_mem_fopen(blob, sz);
	}

	if (fname[0] == '/')
		return fopen(fname, mode);
	snprintf(path, sizeof(path), "%s/%s", gw_nx_root, fname);
	return fopen(path, mode);
}

#elif !defined(__SDLSHIM__)

FILE *fileopen(const char *fname, const char *mode)
{
	return fopen(fname, mode);
}

#else

FILE *fileopen(const char *fname, const char *mode)
{
	return SDLS_fopen(fname, mode);
}

#endif

