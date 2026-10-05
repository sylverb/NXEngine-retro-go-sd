/*
 * SDL 1.2 backend for NXEngine on G&W: LCD present, pad → keys, audio callback pump.
 */
#include <SDL/SDL.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gw_lcd.h"
#include "gw_audio.h"
#include "gw_malloc.h"
#include "gw_mem.h"
#include "gw_pack.h"
#include "odroid_system.h"
#include "odroid_input.h"
#include "odroid_overlay.h"
#include "common.h"

uint32_t HAL_GetTick(void);
void wdog_refresh(void);

static char s_err[64];
static SDL_Surface *s_screen;
static SDL_PixelFormat s_fmt16;
static SDL_PixelFormat s_fmt8;
static SDL_VideoInfo s_vinfo;
static Uint32 s_start_ms;

static SDL_AudioSpec s_audio;
static int s_audio_paused = 1;
static int s_audio_open;
static int s_screen_is_lcd; /* pixels == LCD FB — do not free / do not alloc */

/* Hardware CLUT mirror for LUT8 MapRGB nearest-match. */
static uint32_t s_clut[256];
static int s_clut_count;
static int s_clut_ready;

#define SDL_GW_EXTERNAL_PIXELS  0x01u
#define SDL_GW_LCD_PIXELS       0x02u
#define SDL_GW_BOTTOMUP         0x10000000u  /* BMP storage order — blit/scale flips Y */
#define SDL_GW_STATIC_PALETTE   0x20000000u  /* format->palette is not heap */

/* Pad → SDL key queue (edge detect). VOLUME/PAUSE is Retro-Go only. */
static uint32_t s_pad_prev;
static SDL_Event s_evt_q[16];
static int s_evt_r, s_evt_w;

static odroid_dialog_choice_t s_pause_options[] = {
    ODROID_DIALOG_CHOICE_LAST
};

static void set_err(const char *m)
{
    snprintf(s_err, sizeof(s_err), "%s", m ? m : "");
}

static void evt_push(const SDL_Event *e)
{
    int n = (s_evt_w + 1) & 15;
    if (n == s_evt_r)
        return;
    s_evt_q[s_evt_w] = *e;
    s_evt_w = n;
}

static void push_key(Uint8 type, SDLKey sym)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = type;
    e.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.sym = sym;
    evt_push(&e);
}

static int gw_lcd_is_lut8(void)
{
    return lcd_get_mode() == LCD_MODE_LUT8;
}

static void gw_apply_clut_from_palette(const SDL_Color *colors, int ncolors)
{
    if (!gw_lcd_is_lut8() || !colors || ncolors <= 0)
        return;
    /* Slots 240..255 are reserved for recolored font glyphs (SDL_GW_AllocColor). */
    if (ncolors > 240)
        ncolors = 240;
    uint32_t font_save[16];
    memcpy(font_save, &s_clut[240], sizeof(font_save));
    for (int i = 0; i < ncolors; i++) {
        s_clut[i] = ((uint32_t)colors[i].r << 16) |
                    ((uint32_t)colors[i].g << 8) |
                    (uint32_t)colors[i].b;
    }
    for (int i = ncolors; i < 240; i++)
        s_clut[i] = 0;
    memcpy(&s_clut[240], font_save, sizeof(font_save));
    s_clut_count = 256;
    s_clut_ready = 1;
    /* Program full 256 so font slots and darkened-twin layout stay stable. */
    lcd_set_clut(s_clut, 256);
}

static uint8_t gw_nearest_clut_index(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_clut_ready || s_clut_count <= 0)
        return 0;
    /* Exact black → index 0 (Cave Story colorkey). */
    if (r == 0 && g == 0 && b == 0)
        return 0;
    int best = 0;
    int bestd = 1 << 30;
    int n = s_clut_count > 256 ? 256 : s_clut_count;
    for (int i = 0; i < n; i++) {
        int cr = (int)((s_clut[i] >> 16) & 0xff);
        int cg = (int)((s_clut[i] >> 8) & 0xff);
        int cb = (int)(s_clut[i] & 0xff);
        int dr = cr - (int)r, dg = cg - (int)g, db = cb - (int)b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bestd) {
            bestd = d;
            best = i;
            if (d == 0)
                break;
        }
    }
    return (uint8_t)best;
}

static void gw_blit_surface_to_fb(SDL_Surface *screen, void *fb)
{
    if (!screen || !screen->pixels || !fb)
        return;

    int w = screen->w < GW_LCD_WIDTH ? screen->w : GW_LCD_WIDTH;
    int h = screen->h < GW_LCD_HEIGHT ? screen->h : GW_LCD_HEIGHT;
    int bpp = screen->format->BytesPerPixel;
    int lut8 = gw_lcd_is_lut8();

    if (lut8 && bpp == 1) {
        if (w == GW_LCD_WIDTH && screen->pitch == GW_LCD_WIDTH) {
            memcpy(fb, screen->pixels, (size_t)w * (size_t)h);
        } else {
            for (int y = 0; y < h; y++) {
                memcpy((uint8_t *)fb + y * GW_LCD_WIDTH,
                       (uint8_t *)screen->pixels + y * screen->pitch,
                       (size_t)w);
            }
        }
    } else if (!lut8 && bpp == 2) {
        uint16_t *dst = (uint16_t *)fb;
        for (int y = 0; y < h; y++) {
            memcpy(dst + y * GW_LCD_WIDTH,
                   (uint8_t *)screen->pixels + y * screen->pitch,
                   (size_t)w * 2);
        }
    } else if (!lut8 && bpp == 1 && screen->format->palette) {
        SDL_Color *pal = screen->format->palette->colors;
        uint16_t *dstfb = (uint16_t *)fb;
        for (int y = 0; y < h; y++) {
            uint8_t *src = (uint8_t *)screen->pixels + y * screen->pitch;
            uint16_t *dst = dstfb + y * GW_LCD_WIDTH;
            for (int x = 0; x < w; x++) {
                SDL_Color c = pal[src[x]];
                dst[x] = (uint16_t)SDL_MapRGB(&s_fmt16, c.r, c.g, c.b);
            }
        }
    }
}

static void gw_menu_repaint(void)
{
    /*
     * Firmware does lcd_clear_active_buffer() then this callback before
     * darkening + drawing the pause chrome. Empty repaint ⇒ black menu.
     */
    gw_blit_surface_to_fb(s_screen, lcd_get_active_buffer());
}

static void poll_pad_to_keys_from(const odroid_gamepad_state_t *j)
{
    uint32_t cur = 0;
#define BIT(b) (1u << (b))
    if (j->values[ODROID_INPUT_LEFT])   cur |= BIT(0);
    if (j->values[ODROID_INPUT_RIGHT])  cur |= BIT(1);
    if (j->values[ODROID_INPUT_UP])     cur |= BIT(2);
    if (j->values[ODROID_INPUT_DOWN])   cur |= BIT(3);
    if (j->values[ODROID_INPUT_A])      cur |= BIT(4);
    if (j->values[ODROID_INPUT_B])      cur |= BIT(5);
    if (j->values[ODROID_INPUT_START])  cur |= BIT(6);  /* GAME */
    if (j->values[ODROID_INPUT_SELECT]) cur |= BIT(7);  /* TIME */
    if (j->values[ODROID_INPUT_X])      cur |= BIT(8);
    if (j->values[ODROID_INPUT_Y])      cur |= BIT(9);
    /* ODROID_INPUT_VOLUME = PAUSE/SET — Retro-Go menu only. */

    static const SDLKey map[] = {
        SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN,
        SDLK_x, SDLK_z, SDLK_RETURN, SDLK_ESCAPE,
        SDLK_a, SDLK_s
    };
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        uint32_t m = BIT(i);
        if ((cur & m) && !(s_pad_prev & m))
            push_key(SDL_KEYDOWN, map[i]);
        if (!(cur & m) && (s_pad_prev & m))
            push_key(SDL_KEYUP, map[i]);
    }
    s_pad_prev = cur;
#undef BIT
}

/*
 * Input + wdog. Call common_emu_input_loop only when no LCD swap is pending
 * (firmware contract). Do NOT pace audio/frames here — Flip owns that.
 */
static void gw_sdl_input_service(void)
{
    wdog_refresh();
    odroid_gamepad_state_t j;
    odroid_input_read_gamepad(&j);
    if (!lcd_is_swap_pending())
        common_emu_input_loop(&j, s_pause_options, &gw_menu_repaint);
    poll_pad_to_keys_from(&j);
}

static void gw_sdl_service(void)
{
    gw_sdl_input_service();
}

static void gw_memset_wdog(void *dst, int c, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    while (n > 0) {
        size_t chunk = n > 4096 ? 4096 : n;
        memset(p, c, chunk);
        p += chunk;
        n -= chunk;
        wdog_refresh();
    }
}

/* Flash XIP sheets are read-only on real HW (emulator often ignores writes).
 * Copy to RAM before FillRect / blit-dest / lock — but NEVER for large sheets
 * (tileset/NPC 60–76 KiB); that COW starves the music pool. */
static int gw_surface_make_writable(SDL_Surface *s)
{
    if (!s || !s->pixels)
        return -1;
    if (!(s->flags & SDL_GW_EXTERNAL_PIXELS))
        return 0;
    if (s->flags & SDL_GW_LCD_PIXELS)
        return 0;

    size_t nbytes = (size_t)s->pitch * (size_t)s->h;
    if (!nbytes)
        nbytes = 1;
    /* Small glyph/font surfaces only — large XIP stays read-only. */
    if (nbytes > 8192u) {
        return -1;
    }
    uint8_t *ram = (uint8_t *)gw_alloc(nbytes);
    if (!ram) {
        printf("SDL: COW OOM %dx%d pitch=%u (%u B)\n",
               s->w, s->h, (unsigned)s->pitch, (unsigned)nbytes);
        return -1;
    }

    if (s->flags & SDL_GW_BOTTOMUP) {
        int row = s->pitch;
        for (int y = 0; y < s->h; y++) {
            const uint8_t *src =
                (const uint8_t *)s->pixels + (size_t)(s->h - 1 - y) * (size_t)row;
            memcpy(ram + (size_t)y * (size_t)row, src, (size_t)row);
            if ((y & 15) == 0)
                wdog_refresh();
        }
        s->flags &= ~SDL_GW_BOTTOMUP;
    } else {
        memcpy(ram, s->pixels, nbytes);
    }

    s->pixels = ram;
    s->flags &= ~SDL_GW_EXTERNAL_PIXELS;
    return 0;
}

static int gw_is_ahb_ptr(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    return (a >= 0x30000000u && a < 0x30020000u);
}

static void gw_free_ahb_only(void *p)
{
    if (p && gw_is_ahb_ptr(p))
        free(p);
}

int SDL_Init(Uint32 flags)
{
    (void)flags;
    memset(&s_fmt16, 0, sizeof(s_fmt16));
    s_fmt16.BitsPerPixel = 16;
    s_fmt16.BytesPerPixel = 2;
    s_fmt16.Rmask = 0xF800;
    s_fmt16.Gmask = 0x07E0;
    s_fmt16.Bmask = 0x001F;
    s_fmt16.Rshift = 11;
    s_fmt16.Gshift = 5;
    s_fmt16.Bshift = 0;

    memset(&s_fmt8, 0, sizeof(s_fmt8));
    s_fmt8.BitsPerPixel = 8;
    s_fmt8.BytesPerPixel = 1;

    if (gw_lcd_is_lut8())
        s_vinfo.vfmt = &s_fmt8;
    else
        s_vinfo.vfmt = &s_fmt16;
    s_start_ms = HAL_GetTick();
    s_evt_r = s_evt_w = 0;
    s_pad_prev = 0;
    s_clut_ready = 0;
    s_clut_count = 0;
    return 0;
}

void SDL_Quit(void)
{
    if (s_screen) {
        SDL_FreeSurface(s_screen);
        s_screen = NULL;
    }
    SDL_CloseAudio();
}

const char *SDL_GetError(void) { return s_err; }

char *SDL_GetKeyName(SDLKey key)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "k%d", (int)key);
    return buf;
}

SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, Uint32 flags)
{
    (void)flags;
    (void)bpp;
    w = GW_LCD_WIDTH;
    h = GW_LCD_HEIGHT;
    int lut8 = gw_lcd_is_lut8();

    if (s_screen) {
        s_screen_is_lcd = 0;
        SDL_FreeSurface(s_screen);
        s_screen = NULL;
    }

    s_screen = (SDL_Surface *)calloc(1, sizeof(SDL_Surface));
    if (!s_screen)
        return NULL;
    s_screen->format = (SDL_PixelFormat *)calloc(1, sizeof(SDL_PixelFormat));
    if (!s_screen->format) {
        free(s_screen);
        s_screen = NULL;
        return NULL;
    }
    if (lut8) {
        *s_screen->format = s_fmt8;
        s_screen->format->palette = (SDL_Palette *)calloc(1, sizeof(SDL_Palette));
        if (s_screen->format->palette) {
            s_screen->format->palette->ncolors = 256;
            s_screen->format->palette->colors =
                (SDL_Color *)calloc(256, sizeof(SDL_Color));
        }
        s_screen->pitch = (Uint16)w;
        s_vinfo.vfmt = &s_fmt8;
    } else {
        *s_screen->format = s_fmt16;
        s_screen->pitch = (Uint16)(w * 2);
        s_vinfo.vfmt = &s_fmt16;
    }
    s_screen->w = w;
    s_screen->h = h;
    /* Draw into cached RAM; Flip copies to the LCD framebuffer once. */
    {
        size_t nbytes = lut8 ? (size_t)w * (size_t)h : (size_t)w * (size_t)h * 2;
        s_screen->pixels = gw_alloc(nbytes);
        if (!s_screen->pixels) {
            printf("SDL_SetVideoMode: OOM backbuffer %u B\n", (unsigned)nbytes);
            if (s_screen->format->palette) {
                free(s_screen->format->palette->colors);
                free(s_screen->format->palette);
            }
            free(s_screen->format);
            free(s_screen);
            s_screen = NULL;
            return NULL;
        }
        gw_memset_wdog(s_screen->pixels, 0, nbytes);
    }
    s_screen->flags = SDL_HWPALETTE;
    s_screen->clip_rect.w = (Uint16)w;
    s_screen->clip_rect.h = (Uint16)h;
    s_screen->refcount = 1;
    s_screen_is_lcd = 0;
    return s_screen;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    (void)flags;
    (void)Amask;
    /*
     * Session-lifetime surfaces (font glyphs): bump pools. Never Free'd for
     * real. Stage XIP sheets use CreateRGBSurfaceFrom (AHB headers).
     */
    SDL_Surface *s = (SDL_Surface *)gw_calloc(1, sizeof(SDL_Surface));
    if (!s)
        return NULL;
    s->format = (SDL_PixelFormat *)gw_calloc(1, sizeof(SDL_PixelFormat));
    if (!s->format) {
        gw_free_ahb(s);
        return NULL;
    }
    if (depth == 8) {
        *s->format = s_fmt8;
        /* No per-surface 256-color palette — LUT8 screen CLUT is global.
         * (A 1 KiB palette × hundreds of font glyphs exhausted AHB/DTCM.) */
        s->format->palette = NULL;
    } else {
        *s->format = s_fmt16;
        s->format->Rmask = Rmask ? Rmask : s_fmt16.Rmask;
        s->format->Gmask = Gmask ? Gmask : s_fmt16.Gmask;
        s->format->Bmask = Bmask ? Bmask : s_fmt16.Bmask;
    }
    s->w = width;
    s->h = height;
    s->pitch = (Uint16)(width * s->format->BytesPerPixel);
    if (depth == 8)
        s->pitch = (Uint16)((width + 3) & ~3);
    size_t nbytes = (size_t)s->pitch * (size_t)height;
    s->pixels = gw_alloc(nbytes ? nbytes : 1);
    if (!s->pixels) {
        gw_free_ahb(s->format);
        gw_free_ahb(s);
        set_err("OOM surface");
        printf("SDL: OOM %dx%d*%d (%u B) ahb=%u dtc=%u ram=%u\n",
               width, height, depth, (unsigned)nbytes,
               (unsigned)ahb_get_free_size(),
               (unsigned)dtc_get_free_size(),
               (unsigned)ram_get_free_size());
        return NULL;
    }
    gw_memset_wdog(s->pixels, 0, nbytes);
    s->clip_rect.w = (Uint16)width;
    s->clip_rect.h = (Uint16)height;
    s->refcount = 1;
    return s;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth,
                                      int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    (void)Amask;
    /*
     * XIP sheet wrappers — AHB so Sprites::FlushSheets / delete actually
     * reclaim (~sizeof Surface+Format each). Pixels stay in flash.
     */
    SDL_Surface *s = (SDL_Surface *)gw_calloc_ahb(1, sizeof(SDL_Surface));
    if (!s)
        return NULL;
    s->format = (SDL_PixelFormat *)gw_calloc_ahb(1, sizeof(SDL_PixelFormat));
    if (!s->format) {
        gw_free_ahb(s);
        return NULL;
    }
    if (depth == 8) {
        *s->format = s_fmt8;
    } else {
        *s->format = s_fmt16;
        s->format->Rmask = Rmask ? Rmask : s_fmt16.Rmask;
        s->format->Gmask = Gmask ? Gmask : s_fmt16.Gmask;
        s->format->Bmask = Bmask ? Bmask : s_fmt16.Bmask;
    }
    s->w = width;
    s->h = height;
    s->pitch = (Uint16)pitch;
    s->pixels = pixels;
    s->flags = SDL_GW_EXTERNAL_PIXELS;
    s->clip_rect.w = (Uint16)width;
    s->clip_rect.h = (Uint16)height;
    s->refcount = 1;
    return s;
}

void SDL_FreeSurface(SDL_Surface *surface)
{
    if (!surface)
        return;
    if (surface->refcount > 1) {
        surface->refcount--;
        return;
    }
    if (surface == s_screen) {
        s_screen = NULL;
        s_screen_is_lcd = 0;
    }
    if (surface->pixels && !(surface->flags & SDL_GW_EXTERNAL_PIXELS) &&
        !(surface->flags & SDL_GW_LCD_PIXELS)) {
        gw_free_ahb_only(surface->pixels);
    }
    if (surface->format) {
        if (surface->format->palette &&
            !(surface->flags & SDL_GW_STATIC_PALETTE)) {
            gw_free_ahb_only(surface->format->palette->colors);
            gw_free_ahb_only(surface->format->palette);
        }
        gw_free_ahb_only(surface->format);
    }
    if (surface->gw_index_remap)
        gw_free_ahb_only(surface->gw_index_remap);
    gw_free_ahb_only(surface);
}

int SDL_LockSurface(SDL_Surface *surface)
{
    if (surface && gw_surface_make_writable(surface) != 0)
        return -1;
    return 0;
}
void SDL_UnlockSurface(SDL_Surface *surface) { (void)surface; }

/* Clip src/dst rects so the inner blit has no per-pixel bounds checks. */
static int gw_clip_blit(SDL_Surface *src, SDL_Rect *sr, SDL_Surface *dst, SDL_Rect *dr)
{
    int sx = sr->x, sy = sr->y, sw = (int)sr->w, sh = (int)sr->h;
    int dx = dr->x, dy = dr->y;

    if (sx < 0) {
        dx -= sx;
        sw += sx;
        sx = 0;
    }
    if (sy < 0) {
        dy -= sy;
        sh += sy;
        sy = 0;
    }
    if (sx + sw > src->w)
        sw = src->w - sx;
    if (sy + sh > src->h)
        sh = src->h - sy;

    if (dx < 0) {
        sx -= dx;
        sw += dx;
        dx = 0;
    }
    if (dy < 0) {
        sy -= dy;
        sh += dy;
        dy = 0;
    }
    if (dx + sw > dst->w)
        sw = dst->w - dx;
    if (dy + sh > dst->h)
        sh = dst->h - dy;

    if (sw <= 0 || sh <= 0)
        return 0;

    sr->x = (Sint16)sx;
    sr->y = (Sint16)sy;
    sr->w = (Uint16)sw;
    sr->h = (Uint16)sh;
    dr->x = (Sint16)dx;
    dr->y = (Sint16)dy;
    return 1;
}

static void blit8_copy_rows(const uint8_t *src, int spitch,
                            uint8_t *dst, int dpitch, int w, int h)
{
    for (int y = 0; y < h; y++) {
        memcpy(dst, src, (size_t)w);
        src += spitch;
        dst += dpitch;
    }
}

/* Colorkey: copy opaque runs with memcpy (typical Cave Story sprites). */
static void blit8_colorkey_rows(const uint8_t *src, int spitch,
                                uint8_t *dst, int dpitch,
                                int w, int h, uint8_t key)
{
    for (int y = 0; y < h; y++) {
        const uint8_t *s = src;
        uint8_t *d = dst;
        int x = 0;
        while (x < w) {
            while (x < w && s[x] == key)
                x++;
            int start = x;
            while (x < w && s[x] != key)
                x++;
            if (x > start)
                memcpy(d + start, s + start, (size_t)(x - start));
        }
        src += spitch;
        dst += dpitch;
    }
}

static void blit8_remap_rows(const uint8_t *src, int spitch,
                             uint8_t *dst, int dpitch,
                             int w, int h, const uint8_t *remap,
                             int use_key, uint8_t key)
{
    for (int y = 0; y < h; y++) {
        const uint8_t *s = src;
        uint8_t *d = dst;
        if (!use_key) {
            for (int x = 0; x < w; x++)
                d[x] = remap[s[x]];
        } else {
            /* Skip transparent runs (typical sprite sheets). */
            int x = 0;
            while (x < w) {
                while (x < w && remap[s[x]] == key)
                    x++;
                while (x < w) {
                    uint8_t p = remap[s[x]];
                    if (p == key)
                        break;
                    d[x] = p;
                    x++;
                }
            }
        }
        src += spitch;
        dst += dpitch;
    }
}

int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (!src || !dst || !src->pixels || !dst->pixels || !src->format || !dst->format)
        return -1;
    if (gw_surface_make_writable(dst) != 0)
        return -1;

    SDL_Rect s = {0, 0, (Uint16)src->w, (Uint16)src->h};
    SDL_Rect d = {0, 0, 0, 0};
    if (srcrect)
        s = *srcrect;
    if (dstrect) {
        d.x = dstrect->x;
        d.y = dstrect->y;
    }

    int bpp = src->format->BytesPerPixel;
    if (bpp != dst->format->BytesPerPixel)
        return -1;
    if (!gw_clip_blit(src, &s, dst, &d)) {
        if (dstrect) {
            dstrect->w = 0;
            dstrect->h = 0;
        }
        return 0;
    }

    int use_key = (src->flags & SDL_SRCCOLORKEY) != 0;
    uint32_t ckey = src->format->colorkey;

    if (bpp == 1) {
        int spitch = src->pitch;
        const uint8_t *sp = (const uint8_t *)src->pixels;
        if (src->flags & SDL_GW_BOTTOMUP) {
            sp += (size_t)(src->h - 1 - s.y) * (size_t)spitch + (size_t)s.x;
            spitch = -spitch;
        } else {
            sp += (size_t)s.y * (size_t)spitch + (size_t)s.x;
        }
        uint8_t *dp = (uint8_t *)dst->pixels +
                      (size_t)d.y * (size_t)dst->pitch + (size_t)d.x;

        if (src->gw_index_remap) {
            blit8_remap_rows(sp, spitch, dp, dst->pitch, s.w, s.h,
                             src->gw_index_remap, use_key, (uint8_t)ckey);
        } else if (use_key) {
            blit8_colorkey_rows(sp, spitch, dp, dst->pitch, s.w, s.h, (uint8_t)ckey);
        } else {
            blit8_copy_rows(sp, spitch, dp, dst->pitch, s.w, s.h);
        }
    } else {
        /* RGB565 — less common on this port; still row-friendly. */
        int spitch = src->pitch;
        const uint8_t *sp = (const uint8_t *)src->pixels;
        if (src->flags & SDL_GW_BOTTOMUP) {
            sp += (size_t)(src->h - 1 - s.y) * (size_t)spitch + (size_t)s.x * 2;
            spitch = -spitch;
        } else {
            sp += (size_t)s.y * (size_t)spitch + (size_t)s.x * 2;
        }
        uint8_t *dp = (uint8_t *)dst->pixels +
                      (size_t)d.y * (size_t)dst->pitch + (size_t)d.x * 2;
        if (!use_key) {
            for (int y = 0; y < s.h; y++) {
                memcpy(dp, sp, (size_t)s.w * 2);
                sp += spitch;
                dp += dst->pitch;
            }
        } else {
            uint16_t key = (uint16_t)ckey;
            for (int y = 0; y < s.h; y++) {
                const uint16_t *srow = (const uint16_t *)sp;
                uint16_t *drow = (uint16_t *)dp;
                for (int x = 0; x < s.w; x++) {
                    if (srow[x] != key)
                        drow[x] = srow[x];
                }
                sp += spitch;
                dp += dst->pitch;
            }
        }
    }

    if (dstrect) {
        dstrect->w = s.w;
        dstrect->h = s.h;
    }
    return 0;
}

int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
    if (!dst || !dst->pixels || !dst->format)
        return -1;
    if (gw_surface_make_writable(dst) != 0)
        return -1;

    int x = 0, y = 0, w = dst->w, h = dst->h;
    if (dstrect) {
        x = dstrect->x;
        y = dstrect->y;
        w = dstrect->w;
        h = dstrect->h;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > dst->w)
        w = dst->w - x;
    if (y + h > dst->h)
        h = dst->h - y;
    if (w <= 0 || h <= 0)
        return 0;

    int bpp = dst->format->BytesPerPixel;
    if (bpp == 1) {
        uint8_t c = (uint8_t)color;
        uint8_t *row = (uint8_t *)dst->pixels + (size_t)y * (size_t)dst->pitch + (size_t)x;
        if (w == dst->pitch && x == 0) {
            memset(row, c, (size_t)w * (size_t)h);
        } else {
            for (int i = 0; i < h; i++) {
                memset(row, c, (size_t)w);
                row += dst->pitch;
            }
        }
    } else {
        uint16_t c = (uint16_t)color;
        for (int i = 0; i < h; i++) {
            uint16_t *row = (uint16_t *)((uint8_t *)dst->pixels +
                                         (size_t)(y + i) * (size_t)dst->pitch) + x;
            for (int j = 0; j < w; j++)
                row[j] = c;
        }
    }
    return 0;
}

int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key)
{
    if (!surface || !surface->format)
        return -1;
    surface->flags = (surface->flags & ~SDL_SRCCOLORKEY) | (flag & SDL_SRCCOLORKEY);
    surface->format->colorkey = key;
    return 0;
}

int SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha)
{
    if (!surface || !surface->format)
        return -1;
    surface->flags = (surface->flags & ~SDL_SRCALPHA) | (flag & SDL_SRCALPHA);
    surface->format->alpha = alpha;
    return 0;
}

int SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect)
{
    if (!surface)
        return 0;
    if (!rect) {
        surface->clip_rect.x = 0;
        surface->clip_rect.y = 0;
        surface->clip_rect.w = (Uint16)surface->w;
        surface->clip_rect.h = (Uint16)surface->h;
        return 1;
    }
    surface->clip_rect = *rect;
    return 1;
}

int SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors)
{
    if (!surface || !surface->format || !surface->format->palette || !colors)
        return 0;
    for (int i = 0; i < ncolors; i++) {
        int idx = firstcolor + i;
        if (idx >= 0 && idx < surface->format->palette->ncolors)
            surface->format->palette->colors[idx] = colors[i];
    }
    /* Screen (or any full remap) → push to LTDC CLUT. */
    if (surface == s_screen || (surface->flags & SDL_GW_LCD_PIXELS)) {
        int total = firstcolor + ncolors;
        if (total > surface->format->palette->ncolors)
            total = surface->format->palette->ncolors;
        if (total > 0)
            gw_apply_clut_from_palette(surface->format->palette->colors, total);
    }
    return 1;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b)
{
    if (!fmt)
        return 0;
    if (fmt->BytesPerPixel == 1) {
        if (fmt->palette && fmt->palette->colors) {
            int n = fmt->palette->ncolors > 256 ? 256 : fmt->palette->ncolors;
            for (int i = 0; i < n; i++) {
                SDL_Color c = fmt->palette->colors[i];
                if (c.r == r && c.g == g && c.b == b)
                    return (Uint32)i;
            }
            /* nearest in sheet palette */
            int best = 0, bestd = 1 << 30;
            for (int i = 0; i < n; i++) {
                SDL_Color c = fmt->palette->colors[i];
                int dr = (int)c.r - r, dg = (int)c.g - g, db = (int)c.b - b;
                int d = dr * dr + dg * dg + db * db;
                if (d < bestd) {
                    bestd = d;
                    best = i;
                }
            }
            return (Uint32)best;
        }
        return gw_nearest_clut_index(r, g, b);
    }
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/* Reserve CLUT slots 240..255 for recolored bitmap-font glyphs. */
int SDL_GW_AllocColor(Uint8 r, Uint8 g, Uint8 b)
{
    uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    if (r == 0 && g == 0 && b == 0)
        return 0;
    for (int i = 240; i < 256; i++) {
        if (s_clut[i] == rgb)
            return i;
    }
    static int next = 240;
    if (next >= 256)
        next = 240;
    int idx = next++;
    s_clut[idx] = rgb;
    s_clut_count = 256;
    s_clut_ready = 1;
    if (gw_lcd_is_lut8())
        lcd_set_clut(s_clut, 256);
    return idx;
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b)
{
    if (!fmt) {
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        return;
    }
    if (fmt->BytesPerPixel == 1) {
        if (fmt->palette && fmt->palette->colors &&
            (int)pixel < fmt->palette->ncolors) {
            SDL_Color c = fmt->palette->colors[pixel];
            if (r) *r = c.r;
            if (g) *g = c.g;
            if (b) *b = c.b;
            return;
        }
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        return;
    }
    if (r) *r = (Uint8)((pixel >> 8) & 0xF8);
    if (g) *g = (Uint8)((pixel >> 3) & 0xFC);
    if (b) *b = (Uint8)((pixel << 3) & 0xF8);
}

void SDL_WM_SetCaption(const char *title, const char *icon) { (void)title; (void)icon; }
void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask) { (void)icon; (void)mask; }
void SDL_ShowCursor(int toggle) { (void)toggle; }
void SDL_WarpMouse(Uint16 x, Uint16 y) { (void)x; (void)y; }

int SDL_Flip(SDL_Surface *screen)
{
    if (!screen || !screen->pixels)
        return -1;

    bool draw_frame = common_emu_frame_loop();
    gw_sdl_input_service();

    /* Skip present if LTDC still applying previous swap (no VBlank sleep —
     * audio sync below already paces the frame). */
    if (draw_frame && !lcd_is_swap_pending()) {
        gw_blit_surface_to_fb(screen, lcd_get_active_buffer());
        common_ingame_overlay();
        lcd_swap();
    }

    gw_sdl_audio_pump();
    return 0;
}

Uint32 SDL_GetTicks(void)
{
    return HAL_GetTick() - s_start_ms;
}

void SDL_Delay(int ms)
{
    if (ms < 0)
        ms = 0;
    Uint32 start = HAL_GetTick();
    while ((int)(HAL_GetTick() - start) < ms) {
        /* Input/menu only — no frame or audio pacing here. */
        gw_sdl_input_service();
    }
}

int SDL_PollEvent(SDL_Event *event)
{
    gw_sdl_service();
    if (s_evt_r == s_evt_w)
        return 0;
    if (event)
        *event = s_evt_q[s_evt_r];
    s_evt_r = (s_evt_r + 1) & 15;
    return 1;
}

int SDL_WaitEvent(SDL_Event *event)
{
    while (!SDL_PollEvent(event)) {
        wdog_refresh();
        SDL_Delay(1);
    }
    return 1;
}

Uint8 SDL_GetAppState(void)
{
    return SDL_APPACTIVE | SDL_APPINPUTFOCUS;
}

void SDL_PumpEvents(void) { gw_sdl_service(); }

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
    if (!desired || !desired->callback)
        return -1;
    s_audio = *desired;
    if (obtained) {
        *obtained = *desired;
        obtained->size = (Uint32)desired->samples * desired->channels * 2;
    }
    s_audio_open = 1;
    s_audio_paused = 1;
    return 0;
}

void SDL_CloseAudio(void)
{
    s_audio_open = 0;
    s_audio.callback = NULL;
}

void SDL_PauseAudio(int pause_on)
{
    s_audio_paused = pause_on ? 1 : 0;
}

void SDL_LockAudio(void) { }
void SDL_UnlockAudio(void) { }

void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume)
{
    /* Crude: add scaled int16 samples. */
    Sint16 *d = (Sint16 *)dst;
    const Sint16 *s = (const Sint16 *)src;
    Uint32 n = len / 2;
    for (Uint32 i = 0; i < n; i++) {
        int v = (int)d[i] + ((int)s[i] * volume) / SDL_MIX_MAXVOLUME;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        d[i] = (Sint16)v;
    }
}

void gw_sdl_audio_pump(void)
{
    if (!s_audio_open || s_audio_paused || !s_audio.callback)
        return;
    if (common_emu_sound_loop_is_muted()) {
        common_emu_sound_sync(false);
        return;
    }
    int16_t *out = audio_get_active_buffer();
    int out_len = audio_get_buffer_length(); /* mono samples */
    /* sslib mixes stereo S16 into a buffer of samples*channels*2 bytes */
    int stereo_samples = s_audio.samples;
    int bytes = stereo_samples * s_audio.channels * 2;
    static uint8_t mixbuf[4096];
    if (bytes > (int)sizeof(mixbuf))
        bytes = (int)sizeof(mixbuf);
    memset(mixbuf, 0, (size_t)bytes);
    s_audio.callback(s_audio.userdata, mixbuf, bytes);

    const Sint16 *st = (const Sint16 *)mixbuf;
    int pairs = bytes / 4; /* stereo frames */
    int n = out_len < pairs ? out_len : pairs;
    uint8_t vol = common_emu_sound_get_volume();
    for (int i = 0; i < n; i++) {
        int m = ((int)st[i * 2] + (int)st[i * 2 + 1]) / 2;
        m = (m * (int)vol) / 255;
        out[i] = (int16_t)m;
    }
    for (int i = n; i < out_len; i++)
        out[i] = 0;
    common_emu_sound_sync(false);
}

const SDL_VideoInfo *SDL_GetVideoInfo(void) { return &s_vinfo; }

int SDL_SaveBMP(SDL_Surface *surface, const char *file)
{
    (void)surface;
    (void)file;
    return -1;
}

SDL_Surface *SDL_DisplayFormat(SDL_Surface *surface)
{
    if (!surface)
        return NULL;
    /* Already display-compatible (8bpp on LUT8 / 16 on RGB565): no copy. */
    if (s_screen && surface->format->BytesPerPixel == s_screen->format->BytesPerPixel) {
        surface->refcount++;
        return surface;
    }
    SDL_Surface *d = SDL_CreateRGBSurface(0, surface->w, surface->h, 16,
                                          s_fmt16.Rmask, s_fmt16.Gmask, s_fmt16.Bmask, 0);
    if (!d)
        return NULL;
    SDL_BlitSurface(surface, NULL, d, NULL);
    if (surface->flags & SDL_SRCCOLORKEY)
        SDL_SetColorKey(d, SDL_SRCCOLORKEY, surface->format->colorkey);
    return d;
}

/* ---- BMP / Cave Story PBM: XIP from NXPK pack ---- */
static const uint8_t *gw_pack_try(const char *key, uint32_t *size_out, char *path_out, size_t path_cap)
{
    uint32_t sz = 0;
    const uint8_t *p;

    if (!key || !key[0])
        return NULL;
    p = gw_pack_get(key, &sz);
    if (!p || sz == 0)
        return NULL;
    if (path_out && path_cap)
        snprintf(path_out, path_cap, "%s", key);
    if (size_out)
        *size_out = sz;
    return p;
}

static const uint8_t *gw_pack_image(const char *file, uint32_t *size_out, char *path_out, size_t path_cap)
{
    char tmp[160];
    const uint8_t *hit;
    const char *base;

    if (!file)
        return NULL;

    hit = gw_pack_try(file, size_out, path_out, path_cap);
    if (hit)
        return hit;

    if (file[0] != '/') {
        snprintf(tmp, sizeof(tmp), "data/%s", file);
        hit = gw_pack_try(tmp, size_out, path_out, path_cap);
        if (hit)
            return hit;
    }

    /* data/../endpic/pixel.bmp → also try endpic/pixel.bmp explicitly */
    base = strrchr(file, '/');
    base = base ? base + 1 : file;
    if (base[0]) {
        snprintf(tmp, sizeof(tmp), "endpic/%s", base);
        hit = gw_pack_try(tmp, size_out, path_out, path_cap);
        if (hit)
            return hit;
        snprintf(tmp, sizeof(tmp), "data/endpic/%s", base);
        hit = gw_pack_try(tmp, size_out, path_out, path_cap);
        if (hit)
            return hit;
    }

    if (size_out)
        *size_out = 0;
    return NULL;
}

/*
 * Shared scratch for LoadBMP → palette_add (synchronous). After palette_add
 * only gw_index_remap is needed for blit — do not keep 1 KiB/sheet on bump.
 * Safe because FlushSheets deletes sheets; next Load overwrites scratch before
 * its own palette_add.
 */
static SDL_Palette s_xip_scratch_pal;
static SDL_Color s_xip_scratch_colors[256];

static SDL_Surface *gw_surface_xip_8(const uint8_t *pixels, int width, int height, int pitch,
                                     int bottomup, const uint8_t *pal_bgra, int ncolors)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom((void *)pixels, width, height, 8, pitch, 0, 0, 0, 0);
    if (!s)
        return NULL;
    s->flags |= SDL_GW_EXTERNAL_PIXELS | SDL_GW_STATIC_PALETTE;
    if (bottomup)
        s->flags |= SDL_GW_BOTTOMUP;

    memset(s_xip_scratch_colors, 0, sizeof(s_xip_scratch_colors));
    if (pal_bgra) {
        int n = ncolors > 256 ? 256 : ncolors;
        for (int i = 0; i < n; i++) {
            s_xip_scratch_colors[i].b = pal_bgra[i * 4 + 0];
            s_xip_scratch_colors[i].g = pal_bgra[i * 4 + 1];
            s_xip_scratch_colors[i].r = pal_bgra[i * 4 + 2];
        }
    }
    s_xip_scratch_pal.ncolors = 256;
    s_xip_scratch_pal.colors = s_xip_scratch_colors;
    s->format->palette = &s_xip_scratch_pal;
    return s;
}

typedef struct {
    const uint8_t *bmp;
    uint32_t bmp_size;
    uint32_t data_off;
    int width;
    int height;
    int row_bytes;
    int topdown;
    int y_next;
} gw_unpack4_t;

static int gw_unpack4_fill(void *user, uint8_t *buf, uint32_t buf_cap)
{
    gw_unpack4_t *u = (gw_unpack4_t *)user;
    if (u->y_next >= u->height)
        return 0;
    if (buf_cap < (uint32_t)u->width)
        return -1;

    /* Emit one top-down row as 8bpp. */
    int src_y = u->topdown ? u->y_next : (u->height - 1 - u->y_next);
    uint32_t row_off = u->data_off + (uint32_t)src_y * (uint32_t)u->row_bytes;
    if (row_off + (uint32_t)u->row_bytes > u->bmp_size)
        return -1;
    const uint8_t *row = u->bmp + row_off;
    for (int x = 0; x < u->width; x++) {
        uint8_t v = row[x / 2];
        buf[x] = (x & 1) ? (uint8_t)(v & 0x0f) : (uint8_t)(v >> 4);
    }
    u->y_next++;
    return u->width;
}

SDL_Surface *SDL_LoadBMP(const char *file)
{
    char path[160];
    uint32_t size = 0;
    const uint8_t *blob = gw_pack_image(file, &size, path, sizeof(path));
    if (!blob || size < 54) {
        set_err("LoadBMP pack");
        return NULL;
    }

    const uint8_t *hdr = blob;
    if (hdr[0] != 'B' || hdr[1] != 'M') {
        set_err("LoadBMP header");
        return NULL;
    }
    uint32_t data_off = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | (hdr[13] << 24);
    int32_t width = (int32_t)(hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24));
    int32_t height = (int32_t)(hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24));
    uint16_t bpp = (uint16_t)(hdr[28] | (hdr[29] << 8));
    int topdown = 0;
    if (height < 0) {
        height = -height;
        topdown = 1;
    }
    if (width <= 0 || height <= 0 || (bpp != 4 && bpp != 8 && bpp != 24 && bpp != 16)) {
        set_err("LoadBMP fmt");
        return NULL;
    }
    if (data_off >= size) {
        set_err("LoadBMP off");
        return NULL;
    }

    int row_bytes = ((width * bpp + 31) / 32) * 4;
    const uint8_t *pal = (bpp <= 8 && size >= 54 + (uint32_t)(1 << bpp) * 4) ? (blob + 54) : NULL;
    int ncolors = 1 << bpp;

    /* 8bpp: point straight at flash pixel bytes (palette copied small/mutable). */
    if (bpp == 8) {
        return gw_surface_xip_8(blob + data_off, width, height, row_bytes, !topdown, pal, ncolors);
    }

    /* 4bpp: pack should already contain 8bpp under the logical name. */
    if (bpp == 4) {
        static int warned_4bpp;
        if (!warned_4bpp) {
            warned_4bpp = 1;
        }

        uint32_t need = (uint32_t)width * (uint32_t)height;
        uint8_t *ram = (uint8_t *)gw_alloc(need);
        if (!ram) {
            set_err("LoadBMP unpack4 OOM");
            return NULL;
        }
        gw_unpack4_t u = {
            .bmp = blob,
            .bmp_size = size,
            .data_off = data_off,
            .width = width,
            .height = height,
            .row_bytes = row_bytes,
            .topdown = topdown,
            .y_next = 0,
        };
        uint32_t off = 0;
        while (off < need) {
            wdog_refresh();
            int n = gw_unpack4_fill(&u, ram + off, need - off);
            if (n <= 0) {
                set_err("LoadBMP unpack4");
                return NULL;
            }
            off += (uint32_t)n;
        }
        SDL_Surface *s = gw_surface_xip_8(ram, width, height, width, 0, pal, ncolors);
        if (s)
            s->flags &= ~SDL_GW_EXTERNAL_PIXELS;
        return s;
    }

    /* 16/24: still decode into a RAM surface (rare for Cave Story sheets). */
    SDL_Surface *s = SDL_CreateRGBSurface(0, width, height, 16,
                                          s_fmt16.Rmask, s_fmt16.Gmask, s_fmt16.Bmask, 0);
    if (!s)
        return NULL;
    for (int y = 0; y < height; y++) {
        uint32_t row_off = data_off + (uint32_t)y * (uint32_t)row_bytes;
        if (row_off + (uint32_t)row_bytes > size)
            break;
        const uint8_t *row = blob + row_off;
        int dy = topdown ? y : (height - 1 - y);
        wdog_refresh();
        if (bpp == 24) {
            uint16_t *dst = (uint16_t *)((uint8_t *)s->pixels + dy * s->pitch);
            for (int x = 0; x < width; x++) {
                uint8_t b = row[x * 3 + 0], g = row[x * 3 + 1], r = row[x * 3 + 2];
                dst[x] = (uint16_t)SDL_MapRGB(&s_fmt16, r, g, b);
            }
        } else {
            memcpy((uint8_t *)s->pixels + dy * s->pitch, row, (size_t)width * 2);
        }
    }
    return s;
}

/* libc stubs used by NXEngine */
int atexit(void (*func)(void))
{
    (void)func;
    return 0;
}

int putenv(char *string)
{
    (void)string;
    return 0;
}
