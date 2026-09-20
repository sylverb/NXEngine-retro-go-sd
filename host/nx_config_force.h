/* Force host NXEngine config: no SDL_ttf (bitmap font).
 * Included before each translation unit via -include. */
#ifdef CONFIG_ENABLE_TTF
#undef CONFIG_ENABLE_TTF
#endif
