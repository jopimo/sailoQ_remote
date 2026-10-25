#ifndef FONTS_H
#define FONTS_H
#include <stdint.h>

/* Fuente pequena 6x8 (ASCII 32..127), la misma que la ITF3 */
extern const uint8_t font6x8[96][6];

/* Fuentes de cifras grandes (ancho variable). Caracteres: "0123456789.-:" + grado (0xB0) + espacio */
#define FONT_BIG_N 15
typedef struct { uint8_t w; uint16_t off; } glyph_t;
typedef struct {
    uint8_t h;                  /* alto en pixeles */
    uint8_t nb;                 /* bytes por columna = (h + 7) / 8 */
    const char *chars;
    glyph_t g[FONT_BIG_N];
    const uint8_t *data;
} font_big_t;

extern const font_big_t font_med;   /* 16 px */
extern const font_big_t font_big;   /* 26 px */
extern const font_big_t font_huge;  /* 42 px */

#endif
