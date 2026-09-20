
#ifndef _CONFIG_H
#define _CONFIG_H

/* G&W + host: bitmap fonts. Host keeps extractor; G&W uses pre-extracted SD data. */
#define CONFIG_MUTABLE_SCALE
/* #define CONFIG_ENABLE_TTF */
#ifndef NXENGINE_GW
#define CONFIG_DATA_EXTRACTOR
#endif
/* #define CONFIG_OPENGL */

#endif
