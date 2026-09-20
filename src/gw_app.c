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

void wdog_refresh(void);
void HAL_Delay(uint32_t ms);
void gw_mem_init(void);

#ifdef __cplusplus
extern "C" {
#endif
int nx_engine_main(void);
#ifdef __cplusplus
}
#endif

#define APP_ID APPID_HOMEBREW

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
    boot_banner("Cave Story", "loading pack...");
    printf("NXEngine: pools ahb_free=%u dtc_free=%u ram_free=%u\n",
           (unsigned)ahb_get_free_size(),
           (unsigned)dtc_get_free_size(),
           (unsigned)ram_get_free_size());

#ifndef HOST_BUILD
    /* Saves live under /data/homebrew/cavestory_* (same tree as .cfg). */
    (void)odroid_sdcard_mkdir("/data");
    (void)odroid_sdcard_mkdir(GW_NX_SAVE_DIR);
#endif

    /* One SD→flash cache of cavestory.nxpk; subsequent boots hit XIP. */
    odroid_overlay_draw_progress_bar("Cave Story data", 0);
#ifdef HOST_BUILD
    if (gw_pack_host_load("CaveStory/cavestory.nxpk") != 0 &&
        gw_pack_host_load("cavestory.nxpk") != 0) {
        printf("NXEngine: FATAL — missing cavestory.nxpk (make pack-assets)\n");
        boot_banner("missing nxpk", "make pack-assets");
        while (1) {
            wdog_refresh();
            HAL_Delay(100);
        }
    }
#else
    {
        uint32_t pack_sz = 0;
        uint8_t *pack = odroid_overlay_cache_file_in_flash(GW_NXPK_PATH, &pack_sz, false);
        if (!pack || pack_sz == 0) {
            printf("NXEngine: FATAL — missing %s\n", GW_NXPK_PATH);
            boot_banner("missing nxpk", GW_NXPK_PATH);
            while (1) {
                wdog_refresh();
                HAL_Delay(100);
            }
        }
        if (gw_pack_init(pack, pack_sz) != 0) {
            printf("NXEngine: FATAL — bad NXPK (%u bytes)\n", (unsigned)pack_sz);
            boot_banner("bad nxpk", "re-run make pack-assets");
            while (1) {
                wdog_refresh();
                HAL_Delay(100);
            }
        }
    }
#endif
    odroid_overlay_draw_progress_bar("Cave Story data", 100);
    boot_banner("Cave Story", "starting...");

    /* Defer SAI start until after heavy init would be nicer, but NXEngine's
     * SSInit expects the DMA path already live for PauseAudio(0). */
    audio_start_playing(GW_NX_SAMPLE_RATE / GW_NX_FPS);
    if (start_paused)
        odroid_audio_mute(true);

    printf("NXEngine: entering main (data /homebrews) ram_free=%u\n",
           (unsigned)ram_get_free_size());
    printf("NXEngine: build marker org-bss-20260920\n");
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
