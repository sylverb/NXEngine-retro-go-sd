/*
 * Cave Story (NXEngine) GWHB linker script.
 *
 * Segment 0 = RAM_EMU (entry + cold .text/.rodata/.data/.bss).
 * Segment 1 = ITCM (.core_itcm) — hot game/blit/script .text (~60 KiB of 64).
 *
 * Hot *data*: font glyphs → DTCM (dtc_*); sprite dirs → one RAM_EMU bump.
 */

INCLUDE gnw_ram_emu.ld
INCLUDE gnw_itcm_core.ld

MEMORY
{
  RAM_EMU (xrw) : ORIGIN = __RAM_EMU_START__, LENGTH = __RAM_EMU_LENGTH__
  ITCM    (xrw) : ORIGIN = __ITCM_CORE_START__, LENGTH = __ITCM_CORE_LENGTH__
}

PHDRS
{
  text PT_LOAD FLAGS(5); /* PF_R | PF_X */
  data PT_LOAD FLAGS(6); /* PF_R | PF_W */
  itcm PT_LOAD FLAGS(5);
}

/* Hot translation units — keep EXCLUDE_FILE and .core_itcm in sync. */
SECTIONS
{
  .core_entry __RAM_EMU_START__ :
  {
    KEEP(*(.core_entry))
    KEEP(*(.gw_core_bridge_probe))

    . = ALIGN(4);
    *(EXCLUDE_FILE(
        *sdl_gw.o */sdl_gw.o */*/sdl_gw.o
        *tsc.o */tsc.o */*/tsc.o
        *map.o */map.o */*/map.o
        *player.o */player.o */*/player.o
        *object.o */object.o */*/object.o
        *sprites.o */sprites.o */*/sprites.o
        *font.o */font.o */*/font.o
        *nxsurface.o */nxsurface.o */*/nxsurface.o
        *caret.o */caret.o */*/caret.o
        *graphics.o */graphics.o */*/graphics.o
        *game.o */game.o */*/game.o
        *statusbar.o */statusbar.o */*/statusbar.o
        *ObjManager.o */ObjManager.o */*/ObjManager.o
        *p_arms.o */p_arms.o */*/p_arms.o
        *input.o */input.o */*/input.o
        *trig.o */trig.o */*/trig.o
        *TextBox.o */TextBox.o */*/TextBox.o
        *ai.o */ai.o */*/ai.o
        *SaveSelect.o */SaveSelect.o */*/SaveSelect.o
        *floattext.o */floattext.o */*/floattext.o
        *stageboss.o */stageboss.o */*/stageboss.o
        *title.o */title.o */*/title.o
        *tileset.o */tileset.o */*/tileset.o
        *sslib.o */sslib.o */*/sslib.o
        *sound.o */sound.o */*/sound.o
        *YesNoPrompt.o */YesNoPrompt.o */*/YesNoPrompt.o
        *pause.o */pause.o */*/pause.o
      ) .text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)

    . = ALIGN(4);
    __CORE_INIT_ARRAY_START__ = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    __CORE_INIT_ARRAY_END__ = .;
  } >RAM_EMU :text

  . = ALIGN(8);
  .data :
  {
    *(.data*)
  } >RAM_EMU :data

  __CORE_CODE_END__ = .;

  . = ALIGN(8);
  .bss (NOLOAD) :
  {
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
  } >RAM_EMU :data

  __CORE_BSS_END__ = .;

  ASSERT(__CORE_BSS_END__ <= __RAM_EMU_END__,
         "Error: homebrew RAM_EMU (code+data+bss) overflows RAM_EMU")

  .core_itcm __ITCM_CORE_START__ :
  {
    FILL(0xe7fee7fe)
    . = . + 0x100;
    FILL(0)

    *sdl_gw.o(.text*) */sdl_gw.o(.text*) */*/sdl_gw.o(.text*)
    *tsc.o(.text*) */tsc.o(.text*) */*/tsc.o(.text*)
    *map.o(.text*) */map.o(.text*) */*/map.o(.text*)
    *player.o(.text*) */player.o(.text*) */*/player.o(.text*)
    *object.o(.text*) */object.o(.text*) */*/object.o(.text*)
    *sprites.o(.text*) */sprites.o(.text*) */*/sprites.o(.text*)
    *font.o(.text*) */font.o(.text*) */*/font.o(.text*)
    *nxsurface.o(.text*) */nxsurface.o(.text*) */*/nxsurface.o(.text*)
    *caret.o(.text*) */caret.o(.text*) */*/caret.o(.text*)
    *graphics.o(.text*) */graphics.o(.text*) */*/graphics.o(.text*)
    *game.o(.text*) */game.o(.text*) */*/game.o(.text*)
    *statusbar.o(.text*) */statusbar.o(.text*) */*/statusbar.o(.text*)
    *ObjManager.o(.text*) */ObjManager.o(.text*) */*/ObjManager.o(.text*)
    *p_arms.o(.text*) */p_arms.o(.text*) */*/p_arms.o(.text*)
    *input.o(.text*) */input.o(.text*) */*/input.o(.text*)
    *trig.o(.text*) */trig.o(.text*) */*/trig.o(.text*)
    *TextBox.o(.text*) */TextBox.o(.text*) */*/TextBox.o(.text*)
    *ai.o(.text*) */ai.o(.text*) */*/ai.o(.text*)
    *SaveSelect.o(.text*) */SaveSelect.o(.text*) */*/SaveSelect.o(.text*)
    *floattext.o(.text*) */floattext.o(.text*) */*/floattext.o(.text*)
    *stageboss.o(.text*) */stageboss.o(.text*) */*/stageboss.o(.text*)
    *title.o(.text*) */title.o(.text*) */*/title.o(.text*)
    *tileset.o(.text*) */tileset.o(.text*) */*/tileset.o(.text*)
    *sslib.o(.text*) */sslib.o(.text*) */*/sslib.o(.text*)
    *sound.o(.text*) */sound.o(.text*) */*/sound.o(.text*)
    *YesNoPrompt.o(.text*) */YesNoPrompt.o(.text*) */*/YesNoPrompt.o(.text*)
    *pause.o(.text*) */pause.o(.text*) */*/pause.o(.text*)

    *(.itcm_text*)

    . = ALIGN(4);
    __CORE_ITCM_CODE_END__ = .;
  } >ITCM AT>RAM_EMU :itcm

  __CORE_ITCM_BSS_END__ = __CORE_ITCM_CODE_END__;

  ASSERT(__CORE_ITCM_BSS_END__ <= __ITCM_CORE_START__ + __ITCM_CORE_LENGTH__,
         "Error: hot .text overflows ITCM (64 KiB)")

  __exidx_start = .;
  __exidx_end = .;

  /DISCARD/ :
  {
    *(.fini_array*)
    *(.preinit_array*)
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.comment)
    *(.ARM.attributes)
  }
}
