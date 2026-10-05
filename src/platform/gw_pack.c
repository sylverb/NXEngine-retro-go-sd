/*
 * NXPK pack + memfile stdio wrappers (--wrap core_* on device, fread on host).
 */
#include "gw_pack.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "gw_nx_config.h"

#ifdef GW_PACK_STDIO_WRAP
#ifndef HOST_BUILD
#include "gw_core_bridge.h"
extern void wdog_refresh(void);
#endif
#endif

#define GW_ENTRY_STRIDE (GW_NXPK_PATH_LEN + 8)
#define GW_MEMFILE_SLOTS 12

typedef struct {
    int in_use;
    const uint8_t *data;
    uint32_t size;
    uint32_t pos;
    int eof;
} gw_memfile_t;

static const uint8_t *s_base;
static uint32_t s_size;
static uint16_t s_count;
static gw_memfile_t s_slots[GW_MEMFILE_SLOTS];

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int path_eq_ci(const char *a, const char *b)
{
    for (;;) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z')
            ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
        if (ca == 0)
            return 1;
    }
}

/* Strip SD root / leading ./ and resolve a/../b so lookup keys match pack TOC.
 * sprites.sif uses data/../endpic/pixel.bmp for the Pixel credits portrait. */
static void normalize_pack_path(const char *in, char *buf, size_t buflen)
{
    const char *p = in ? in : "";
    char tmp[GW_NXPK_PATH_LEN];
    char *parts[24];
    int nparts = 0;

    while (p[0] == '.' && p[1] == '/')
        p += 2;

    static const char *roots[] = {
        GW_NX_DATA_ROOT "/",
        "/roms/homebrew/cavestory/",
        NULL,
    };
    for (int i = 0; roots[i]; i++) {
        size_t n = strlen(roots[i]);
        if (strncmp(p, roots[i], n) == 0) {
            p += n;
            break;
        }
    }
    while (*p == '/')
        p++;

    snprintf(tmp, sizeof(tmp), "%s", p);
    for (char *tok = strtok(tmp, "/"); tok; tok = strtok(NULL, "/")) {
        if (tok[0] == '\0' || (tok[0] == '.' && tok[1] == '\0'))
            continue;
        if (tok[0] == '.' && tok[1] == '.' && tok[2] == '\0') {
            if (nparts > 0)
                nparts--;
            continue;
        }
        if (nparts < (int)(sizeof(parts) / sizeof(parts[0])))
            parts[nparts++] = tok;
    }

    buf[0] = '\0';
    for (int i = 0; i < nparts; i++) {
        if (i)
            strncat(buf, "/", buflen - strlen(buf) - 1);
        strncat(buf, parts[i], buflen - strlen(buf) - 1);
    }
}

int gw_pack_init(const uint8_t *base, uint32_t size)
{
    s_base = NULL;
    s_size = 0;
    s_count = 0;
    memset(s_slots, 0, sizeof(s_slots));

    if (!base || size < 8)
        return -1;
    if (base[0] != 'N' || base[1] != 'X' || base[2] != 'P' || base[3] != 'K')
        return -1;

    uint16_t ver = (uint16_t)(base[4] | (base[5] << 8));
    uint16_t count = (uint16_t)(base[6] | (base[7] << 8));
    if (ver != 1 || count == 0)
        return -1;

    uint32_t toc_bytes = (uint32_t)count * GW_ENTRY_STRIDE;
    if (8u + toc_bytes > size)
        return -1;

    /* Validate every TOC range — a stale flash-cache hit can look like
     * NXPK at the header but have garbage offsets past EOF. */
    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *ent = base + 8 + (uint32_t)i * GW_ENTRY_STRIDE;
        uint32_t off = rd_u32(ent + GW_NXPK_PATH_LEN);
        uint32_t sz = rd_u32(ent + GW_NXPK_PATH_LEN + 4);
        if (off < 8u + toc_bytes || (uint64_t)off + sz > size)
            return -1;
    }

    s_base = base;
    s_size = size;
    s_count = count;
    printf("gw_pack: ready %u entries, %u bytes @ %p\n",
           (unsigned)count, (unsigned)size, (const void *)base);
    return 0;
}

bool gw_pack_ready(void)
{
    return s_base != NULL && s_count > 0;
}

bool gw_pack_ptr_in_pack(const void *p)
{
    if (!p || !s_base || s_size == 0)
        return false;
    uintptr_t a = (uintptr_t)p;
    uintptr_t b = (uintptr_t)s_base;
    return a >= b && a < (b + (uintptr_t)s_size);
}

const uint8_t *gw_pack_get(const char *relpath, uint32_t *size_out)
{
    char key[GW_NXPK_PATH_LEN];
    if (size_out)
        *size_out = 0;
    if (!gw_pack_ready() || !relpath)
        return NULL;

    normalize_pack_path(relpath, key, sizeof(key));
    if (!key[0])
        return NULL;

#ifndef HOST_BUILD
#ifdef GW_PACK_STDIO_WRAP
    wdog_refresh();
#endif
#endif

    for (uint16_t i = 0; i < s_count; i++) {
        const uint8_t *ent = s_base + 8 + (uint32_t)i * GW_ENTRY_STRIDE;
        const char *path = (const char *)ent;
        if (!path_eq_ci(path, key))
            continue;
        uint32_t off = rd_u32(ent + GW_NXPK_PATH_LEN);
        uint32_t sz = rd_u32(ent + GW_NXPK_PATH_LEN + 4);
        if ((uint64_t)off + sz > s_size)
            return NULL;
        if (size_out)
            *size_out = sz;
        return s_base + off;
    }
    return NULL;
}

static gw_memfile_t *slot_from_file(FILE *fp)
{
    if (!fp)
        return NULL;
    uintptr_t a = (uintptr_t)fp;
    uintptr_t b = (uintptr_t)&s_slots[0];
    uintptr_t e = (uintptr_t)&s_slots[GW_MEMFILE_SLOTS];
    if (a < b || a >= e)
        return NULL;
    if ((a - b) % sizeof(gw_memfile_t) != 0)
        return NULL;
    gw_memfile_t *mf = (gw_memfile_t *)fp;
    return mf->in_use ? mf : NULL;
}

bool gw_pack_is_memfile(FILE *fp)
{
    return slot_from_file(fp) != NULL;
}

FILE *gw_pack_mem_fopen(const uint8_t *data, uint32_t size)
{
    if (!data)
        return NULL;
    for (int i = 0; i < GW_MEMFILE_SLOTS; i++) {
        if (!s_slots[i].in_use) {
            s_slots[i].in_use = 1;
            s_slots[i].data = data;
            s_slots[i].size = size;
            s_slots[i].pos = 0;
            s_slots[i].eof = 0;
            return (FILE *)&s_slots[i];
        }
    }
    printf("gw_pack: memfile slots exhausted\n");
    return NULL;
}

/* ---- stdio wraps (device / optional host) ------------------------------ */

#ifdef GW_PACK_STDIO_WRAP

static size_t mem_fread(void *ptr, size_t size, size_t nmemb, gw_memfile_t *mf)
{
    /* POSIX: size or nmemb == 0 → return 0, stream state unchanged.
     * drum.pcm has empty slots (nsamples=0); a zero-length fread must not
     * sticky-set EOF or the next fgetl() sees feof and exit()s to the launcher. */
    if (!size || !nmemb)
        return 0;
    if (!ptr) {
        mf->eof = 1;
        return 0;
    }
    size_t want = size * nmemb;
    size_t left = (mf->pos < mf->size) ? (mf->size - mf->pos) : 0;
    size_t got = want < left ? want : left;
    if (got)
        memcpy(ptr, mf->data + mf->pos, got);
    mf->pos += (uint32_t)got;
    if (got < want)
        mf->eof = 1;
    else
        mf->eof = 0;
    return got / size;
}

static int mem_fseek(gw_memfile_t *mf, long offset, int whence)
{
    long np;
    if (whence == SEEK_SET)
        np = offset;
    else if (whence == SEEK_CUR)
        np = (long)mf->pos + offset;
    else if (whence == SEEK_END)
        np = (long)mf->size + offset;
    else
        return -1;
    if (np < 0 || (uint32_t)np > mf->size)
        return -1;
    mf->pos = (uint32_t)np;
    mf->eof = 0;
    return 0;
}

#ifdef HOST_BUILD
extern size_t __real_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int __real_fclose(FILE *stream);
extern int __real_fseek(FILE *stream, long offset, int whence);
extern long __real_ftell(FILE *stream);
extern int __real_feof(FILE *stream);
extern int __real_ferror(FILE *stream);
extern int __real_fgetc(FILE *stream);
extern size_t __real_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

size_t __wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return mem_fread(ptr, size, nmemb, mf);
    return __real_fread(ptr, size, nmemb, stream);
}
int __wrap_fclose(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf) {
        mf->in_use = 0;
        return 0;
    }
    return __real_fclose(stream);
}
int __wrap_fseek(FILE *stream, long offset, int whence)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return mem_fseek(mf, offset, whence);
    return __real_fseek(stream, offset, whence);
}
long __wrap_ftell(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return (long)mf->pos;
    return __real_ftell(stream);
}
int __wrap_feof(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return mf->eof; /* libc: only after a short/failed read, not at EOF position */
    return __real_feof(stream);
}
int __wrap_ferror(FILE *stream)
{
    if (slot_from_file(stream))
        return 0;
    return __real_ferror(stream);
}
int __wrap_fgetc(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf) {
        if (mf->pos >= mf->size) {
            mf->eof = 1;
            return EOF;
        }
        return mf->data[mf->pos++];
    }
    return __real_fgetc(stream);
}
size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (slot_from_file(stream))
        return 0; /* read-only */
    return __real_fwrite(ptr, size, nmemb, stream);
}

#else /* device: wrap core_* after redefine */

extern size_t __real_core_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int __real_core_fclose(FILE *stream);
extern int __real_core_fseek(FILE *stream, long offset, int whence);
extern long __real_core_ftell(FILE *stream);
extern int __real_core_feof(FILE *stream);
extern int __real_core_ferror(FILE *stream);
extern int __real_core_fgetc(FILE *stream);
extern size_t __real_core_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

size_t __wrap_core_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return mem_fread(ptr, size, nmemb, mf);
    return __real_core_fread(ptr, size, nmemb, stream);
}
int __wrap_core_fclose(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf) {
        mf->in_use = 0;
        return 0;
    }
    return __real_core_fclose(stream);
}
int __wrap_core_fseek(FILE *stream, long offset, int whence)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return mem_fseek(mf, offset, whence);
    return __real_core_fseek(stream, offset, whence);
}
long __wrap_core_ftell(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return (long)mf->pos;
    return __real_core_ftell(stream);
}
int __wrap_core_feof(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf)
        return mf->eof; /* libc: only after a short/failed read, not at EOF position */
    return __real_core_feof(stream);
}
int __wrap_core_ferror(FILE *stream)
{
    if (slot_from_file(stream))
        return 0;
    return __real_core_ferror(stream);
}
int __wrap_core_fgetc(FILE *stream)
{
    gw_memfile_t *mf = slot_from_file(stream);
    if (mf) {
        if (mf->pos >= mf->size) {
            mf->eof = 1;
            return EOF;
        }
        return mf->data[mf->pos++];
    }
    return __real_core_fgetc(stream);
}
size_t __wrap_core_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (slot_from_file(stream))
        return 0;
    return __real_core_fwrite(ptr, size, nmemb, stream);
}

#endif /* !HOST_BUILD */
#endif /* GW_PACK_STDIO_WRAP */

/* Host / desktop: load cavestory_<loc>.nxpk into malloc. */
int gw_pack_host_load(const char *path_hint)
{
    static uint8_t *s_host_blob;
    /* Prefer locale-suffixed packs; keep bare cavestory.nxpk as legacy fallback. */
    const char *cands[] = {
        path_hint,
        "CaveStory/cavestory_en.nxpk",
        "cavestory_en.nxpk",
        "sd_content/homebrews/cavestory_en.nxpk",
        "CaveStory/cavestory.nxpk",
        "cavestory.nxpk",
        GW_NX_NXPK_PATH,
        "CaveStory/cavestory_fr.nxpk",
        "cavestory_fr.nxpk",
        "CaveStory/cavestory_ja.nxpk",
        "cavestory_ja.nxpk",
        "CaveStory/cavestory_ko.nxpk",
        "cavestory_ko.nxpk",
        "sd_content/homebrews/cavestory_fr.nxpk",
        "sd_content/homebrews/cavestory_ja.nxpk",
        "sd_content/homebrews/cavestory_ko.nxpk",
        NULL,
    };

    for (int i = 0; cands[i]; i++) {
        if (!cands[i] || !cands[i][0])
            continue;
        FILE *fp = fopen(cands[i], "rb");
        if (!fp)
            continue;
        if (fseek(fp, 0, SEEK_END) != 0) {
            fclose(fp);
            continue;
        }
        long sz = ftell(fp);
        if (sz <= 0) {
            fclose(fp);
            continue;
        }
        rewind(fp);
        free(s_host_blob);
        s_host_blob = (uint8_t *)malloc((size_t)sz);
        if (!s_host_blob) {
            fclose(fp);
            return -1;
        }
        if (fread(s_host_blob, 1, (size_t)sz, fp) != (size_t)sz) {
            fclose(fp);
            free(s_host_blob);
            s_host_blob = NULL;
            return -1;
        }
        fclose(fp);
        if (gw_pack_init(s_host_blob, (uint32_t)sz) != 0) {
            free(s_host_blob);
            s_host_blob = NULL;
            return -1;
        }
        printf("gw_pack: host loaded '%s'\n", cands[i]);
        {
            uint32_t tsz = 0;
            const uint8_t *t = gw_pack_get("data/Head.tsc", &tsz);
            printf("gw_pack: probe data/Head.tsc → %s (%u bytes)\n",
                   t ? "ok" : "MISS", (unsigned)tsz);
        }
        return 0;
    }
    printf("gw_pack: host — no cavestory*.nxpk found (run make pack-assets)\n");
    return -1;
}

