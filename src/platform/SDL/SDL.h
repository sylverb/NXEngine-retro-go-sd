/*
 * SDL 1.2 shim for NXEngine on Game & Watch Retro-Go SD.
 */
#ifndef _SDL_H
#define _SDL_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;

#ifndef SDL_BYTEORDER
#define SDL_LIL_ENDIAN  1234
#define SDL_BIG_ENDIAN  4321
#define SDL_BYTEORDER   SDL_LIL_ENDIAN
#endif

typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

typedef struct SDL_Color {
    Uint8 r, g, b, unused;
} SDL_Color;

typedef struct SDL_Palette {
    int ncolors;
    SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 Rloss, Gloss, Bloss, Aloss;
    Uint8 Rshift, Gshift, Bshift, Ashift;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 colorkey;
    Uint8 alpha;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    Uint16 pitch;
    void *pixels;
    SDL_Rect clip_rect;
    int refcount;
    /* GW LUT8: map sheet indices → shared screen CLUT (NULL = identity). */
    Uint8 *gw_index_remap;
} SDL_Surface;

typedef enum {
    SDL_NOEVENT = 0,
    SDL_ACTIVEEVENT,
    SDL_KEYDOWN,
    SDL_KEYUP,
    SDL_QUIT = 12,
    SDL_VIDEORESIZE = 16,
    SDL_USEREVENT = 24
} SDL_EventType;

/* Enough keycodes for mappings[SDLK_LAST] */
typedef enum {
    SDLK_UNKNOWN = 0,
    SDLK_BACKSPACE = 8,
    SDLK_RETURN = 13,
    SDLK_ESCAPE = 27,
    SDLK_SPACE = 32,
    SDLK_0 = 48, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_a = 97, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
    SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
    SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
    SDLK_UP = 273,
    SDLK_DOWN = 274,
    SDLK_RIGHT = 275,
    SDLK_LEFT = 276,
    SDLK_INSERT = 277,
    SDLK_HOME = 278,
    SDLK_END = 279,
    SDLK_PAGEUP = 280,
    SDLK_PAGEDOWN = 281,
    SDLK_F1 = 282, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
    SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12,
    SDLK_LSHIFT = 304,
    SDLK_RSHIFT = 303,
    SDLK_LCTRL = 306,
    SDLK_LALT = 308,
    SDLK_TAB = 9,
    SDLK_PAUSE = 19,
    SDLK_PLUS = 43,
    SDLK_MINUS = 45,
    SDLK_HASH = 35,
    SDLK_LAST = 322
} SDLKey;

typedef struct SDL_keysym {
    Uint8 scancode;
    SDLKey sym;
    Uint32 mod;
    Uint16 unicode;
} SDL_keysym;

typedef struct SDL_KeyboardEvent {
    Uint8 type;
    Uint8 which;
    Uint8 state;
    SDL_keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_ActiveEvent {
    Uint8 type;
    Uint8 gain;
    Uint8 state;
} SDL_ActiveEvent;

typedef union SDL_Event {
    Uint8 type;
    SDL_ActiveEvent active;
    SDL_KeyboardEvent key;
} SDL_Event;

typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

typedef struct SDL_AudioSpec {
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint32 size;
    SDL_AudioCallback callback;
    void *userdata;
} SDL_AudioSpec;

#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_U16LSB 0x0010
#define AUDIO_S16LSB 0x8010
#define AUDIO_U16MSB 0x1010
#define AUDIO_S16MSB 0x9010
#define AUDIO_U16    AUDIO_U16LSB
#define AUDIO_S16    AUDIO_S16LSB

#define SDL_MIX_MAXVOLUME 128

#define SDL_INIT_TIMER   0x00000001
#define SDL_INIT_AUDIO   0x00000010
#define SDL_INIT_VIDEO   0x00000020
#define SDL_SWSURFACE    0x00000000
#define SDL_HWSURFACE    0x00000001
#define SDL_FULLSCREEN   0x80000000
#define SDL_HWPALETTE    0x20000000
#define SDL_SRCCOLORKEY  0x00001000
#define SDL_SRCALPHA     0x00010000
#define SDL_RLEACCEL     0x00004000
#define SDL_APPACTIVE    0x04
#define SDL_APPINPUTFOCUS 0x02
#define SDL_PRESSED      1
#define SDL_RELEASED     0

int SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char *SDL_GetError(void);
char *SDL_GetKeyName(SDLKey key);

SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, Uint32 flags);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth,
                                      int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
SDL_Surface *SDL_LoadBMP(const char *file);
SDL_Surface *SDL_DisplayFormat(SDL_Surface *surface);
int SDL_SaveBMP(SDL_Surface *surface, const char *file);
void SDL_FreeSurface(SDL_Surface *surface);
int SDL_LockSurface(SDL_Surface *surface);
void SDL_UnlockSurface(SDL_Surface *surface);
int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key);
int SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha);
int SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect);
int SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors);
Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
/* LUT8: allocate/reuse a CLUT index for an RGB888 color (font recolor). */
int SDL_GW_AllocColor(Uint8 r, Uint8 g, Uint8 b);
void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b);
void SDL_WM_SetCaption(const char *title, const char *icon);
void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask);
void SDL_ShowCursor(int toggle);
void SDL_WarpMouse(Uint16 x, Uint16 y);
int SDL_Flip(SDL_Surface *screen);

Uint32 SDL_GetTicks(void);
void SDL_Delay(int ms);
int SDL_PollEvent(SDL_Event *event);
int SDL_WaitEvent(SDL_Event *event);
Uint8 SDL_GetAppState(void);
void SDL_PumpEvents(void);

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
void SDL_CloseAudio(void);
void SDL_PauseAudio(int pause_on);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume);

typedef struct SDL_VideoInfo {
    const SDL_PixelFormat *vfmt;
} SDL_VideoInfo;
const SDL_VideoInfo *SDL_GetVideoInfo(void);

/* Called from the G&W frame glue to feed sslib. */
void gw_sdl_audio_pump(void);

#ifdef __cplusplus
}
#endif

#endif /* _SDL_H */
