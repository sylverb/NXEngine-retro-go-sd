/*
 * Cave Story GWHB — boots firmware services then enters NXEngine.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "gw_lcd.h"
#include "gw_audio.h"
#include "odroid_system.h"
#include "odroid_input.h"
#include "odroid_overlay.h"
#include "appid.h"
#include "gw_malloc.h"

#ifndef HOST_BUILD
#include "gw_core_bridge.h"
#else
#include "host_compat.h"
#endif

#include "platform/gw_nx_config.h"
#include "platform/gw_pack.h"
#include "gw_mem.h"

void wdog_refresh(void);
void HAL_Delay(uint32_t ms);

#ifdef __cplusplus
extern "C" {
#endif
int nx_engine_main(void);
#ifdef __cplusplus
}
#endif

#define APP_ID APPID_HOMEBREW

static void ascii_tolower_copy(char *dst, size_t dstlen, const char *src)
{
    size_t i = 0;
    if (dstlen == 0)
        return;
    for (; src[i] && i + 1 < dstlen; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c - 'A' + 'a');
        dst[i] = (char)c;
    }
    dst[i] = '\0';
}

static const char *basename_of(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return base;
}

char *gw_nx_resolve_nxpk_path(char *buf, size_t buflen)
{
    char stem[64];
    const char *raw = "cavestory";

#ifndef HOST_BUILD
    /* Prefer the SD filename (CaveStory_fr.bin) over the GWHB display
     * name ("Cave Story FR") — the latter is launcher UI only. */
    if (ACTIVE_FILE) {
        if (ACTIVE_FILE->path[0])
            raw = basename_of(ACTIVE_FILE->path);
        else if (ACTIVE_FILE->name[0])
            raw = ACTIVE_FILE->name;
    }
#endif

    ascii_tolower_copy(stem, sizeof(stem), basename_of(raw));
    /* Strip trailing .bin if the launcher stored the full filename. */
    {
        size_t n = strlen(stem);
        if (n > 4 && strcmp(stem + n - 4, ".bin") == 0)
            stem[n - 4] = '\0';
    }

    /* Display-name fallback: "cave story fr" → "cavestory_fr". */
    if (strncmp(stem, "cave story", 10) == 0) {
        const char *rest = stem + 10;
        char loc[8];
        size_t i = 0;

        while (*rest == ' ')
            rest++;
        for (; rest[i] && rest[i] != ' ' && i + 1 < sizeof(loc); i++)
            loc[i] = rest[i];
        loc[i] = '\0';
        if (loc[0])
            snprintf(stem, sizeof(stem), "cavestory_%s", loc);
        else
            snprintf(stem, sizeof(stem), "cavestory");
    }

    if (stem[0] == '\0')
        snprintf(stem, sizeof(stem), "cavestory");

    if (buflen < strlen(GW_NX_DATA_ROOT) + 1 + strlen(stem) + 5 + 1)
        return NULL;
    snprintf(buf, buflen, "%s/%s.nxpk", GW_NX_DATA_ROOT, stem);
    return buf;
}

static bool SaveState(const char *path) { (void)path; return false; }
static bool LoadState(const char *path) { (void)path; return false; }
static void *Screenshot(void)
{
    lcd_wait_for_vblank();
    return lcd_get_active_buffer();
}

static void boot_banner(const char *line1, const char *line2)
{
    void *fb = lcd_get_active_buffer();
    size_t n = (size_t)GW_LCD_WIDTH * (size_t)GW_LCD_HEIGHT;
    int lut8 = (lcd_get_mode() == LCD_MODE_LUT8);

    if (lut8) {
        uint8_t *p = (uint8_t *)fb;
        for (size_t i = 0; i < n; i++) {
            if ((i & 0x3ff) == 0)
                wdog_refresh();
            p[i] = 0;
        }
        for (int x = 0; x < GW_LCD_WIDTH; x++) {
            p[40 * GW_LCD_WIDTH + x] = 255;
            p[200 * GW_LCD_WIDTH + x] = 255;
        }
    } else {
        uint16_t *p = (uint16_t *)fb;
        for (size_t i = 0; i < n; i++) {
            if ((i & 0x3ff) == 0)
                wdog_refresh();
            p[i] = 0;
        }
        for (int x = 0; x < GW_LCD_WIDTH; x++) {
            p[40 * GW_LCD_WIDTH + x] = 0xFFFF;
            p[200 * GW_LCD_WIDTH + x] = 0xFFFF;
        }
    }
    (void)line1;
    (void)line2;
    printf("NXEngine: %s | %s | ram_free=%u\n",
           line1 ? line1 : "", line2 ? line2 : "",
           (unsigned)ram_get_free_size());
    common_ingame_overlay();
    lcd_swap();
}

/* Boot failure: show a short message, wait for a button, return to Retro-Go.
 * Never spin forever — that leaves the device unusable without a battery pull. */
static void fatal_return_to_launcher(const char *line1, const char *line2)
    __attribute__((noreturn));
static void fatal_return_to_launcher(const char *line1, const char *line2)
{
    const uint16_t fg = 0xFFFF;
    const uint16_t bg = 0x0000;

    printf("NXEngine: FATAL — %s | %s | ram_free=%u\n",
           line1 ? line1 : "", line2 ? line2 : "",
           (unsigned)ram_get_free_size());

    odroid_overlay_draw_fill_rect(0, 0, GW_LCD_WIDTH, GW_LCD_HEIGHT, bg);
    odroid_overlay_draw_text(8, 72, GW_LCD_WIDTH - 16,
                             line1 ? line1 : "Error", fg, bg);
    if (line2 && line2[0])
        odroid_overlay_draw_text(8, 96, GW_LCD_WIDTH - 16, line2, fg, bg);
    odroid_overlay_draw_text(8, 152, GW_LCD_WIDTH - 16,
                             "Press any button", fg, bg);
    odroid_overlay_draw_text(8, 176, GW_LCD_WIDTH - 16,
                             "to return to menu", fg, bg);
    lcd_swap();

#ifndef HOST_BUILD
    /* Ignore buttons still held from launch, then wait for a fresh press. */
    for (;;) {
        odroid_gamepad_state_t j;
        wdog_refresh();
        odroid_input_read_gamepad(&j);
        if (j.bitmask == 0)
            break;
        HAL_Delay(16);
    }
    for (;;) {
        odroid_gamepad_state_t j;
        wdog_refresh();
        odroid_input_read_gamepad(&j);
        if (j.bitmask != 0)
            break;
        HAL_Delay(16);
    }
    odroid_system_switch_app(0);
#else
    while (1) {
        wdog_refresh();
        HAL_Delay(100);
    }
#endif
    while (1) {
    }
}

void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
{
    (void)load_state;
    (void)save_slot;

    odroid_system_init(APP_ID, GW_NX_SAMPLE_RATE);
    odroid_system_emu_init(&LoadState, &SaveState, &Screenshot,
                           NULL, NULL, NULL, NULL);

    /* DTCM bump for session-lifetime hot data (sprite dirs, font glyphs). */
    dtc_init();

    /* LUT8: 2×75 KiB FB + ~150 KiB bonus (matches Cave Story 8bpp assets). */
    lcd_setup_framebuffers(LCD_MODE_LUT8);
    lcd_clear_buffers();
    lcd_set_refresh_rate(GW_NX_FPS);
    common_emu_state.frame_time_10us = (int16_t)(100000 / GW_NX_FPS);
    common_emu_state.pause_after_frames = start_paused ? 2 : 0;

    gw_mem_init();
    gw_mem_log("boot");
    boot_banner("Cave Story", "loading pack...");

#ifndef HOST_BUILD
    /* Saves live under /data/homebrew/cavestory_* (same tree as .cfg). */
    (void)odroid_sdcard_mkdir("/data");
    (void)odroid_sdcard_mkdir(GW_NX_SAVE_DIR);
#endif

    /* One SD→flash cache of cavestory_<loc>.nxpk; subsequent boots hit XIP. */
    odroid_overlay_draw_progress_bar("Cave Story data", 0);
#ifdef HOST_BUILD
    if (gw_pack_host_load(NULL) != 0)
        fatal_return_to_launcher("missing nxpk", "make pack-assets");
#else
    {
        char nxpk_path[GW_NX_NXPK_PATH_MAX];
        uint32_t pack_sz = 0;
        uint8_t *pack;

        if (!gw_nx_resolve_nxpk_path(nxpk_path, sizeof(nxpk_path)))
            fatal_return_to_launcher("nxpk path", "too long");
        printf("NXEngine: loading %s\n", nxpk_path);
        pack = odroid_overlay_cache_file_in_flash(nxpk_path, &pack_sz, false);
        if (!pack || pack_sz == 0)
            fatal_return_to_launcher("missing nxpk", nxpk_path);
        /* Circular flash cache can place a hit anywhere in EXTFLASH. Log the
         * absolute address so "works after settings reset" regressions are
         * obvious (old builds freed XIP above 0x91000000 / 0x92000000). */
        printf("NXEngine: nxpk XIP %u bytes @ %p (off 0x%lx)\n",
               (unsigned)pack_sz, (void *)pack,
               (unsigned long)((uintptr_t)pack - 0x90000000u));
        /* Touch first + last byte through the mmap window before trusting TOC. */
        volatile uint8_t probe = pack[0] ^ pack[pack_sz - 1];
        (void)probe;
        if (gw_pack_init(pack, pack_sz) != 0)
            fatal_return_to_launcher("bad nxpk", "clear flash cache");
    }
#endif
    odroid_overlay_draw_progress_bar("Cave Story data", 100);
    boot_banner("Cave Story", "starting...");

    /* Defer SAI until after NXEngine sound_init — starting DMA here nests
     * on the same MSP as deep init and overflows DTCM (~24 KiB). */
    if (start_paused)
        odroid_audio_mute(true);

    int rc = nx_engine_main();
    printf("NXEngine: exited rc=%d\n", rc);

    boot_banner(rc ? "engine error" : "engine exit", "PAUSE/SET for menu");
    while (1) {
        wdog_refresh();
        (void)common_emu_frame_loop();
        odroid_gamepad_state_t j;
        odroid_input_read_gamepad(&j);
        common_emu_input_loop(&j, NULL, NULL);
        HAL_Delay(16);
    }
}
