#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "st7571.h"
#include "fonts.h"

/* Imagen en RAM (2 KB): 16 paginas de 8 filas x 128 columnas, bit 0 = fila de arriba de la pagina */
extern uint8_t gfx_fb[LCD_H / 8][LCD_W];

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

void gfx_clear(void);
void gfx_show(void);                                    /* vuelca a la pantalla */
void gfx_pixel(int x, int y, int on);
void gfx_hline(int x0, int x1, int y, int on);
void gfx_vline(int x, int y0, int y1, int on);
void gfx_line(int x0, int y0, int x1, int y1, int on);
void gfx_rect(int x, int y, int w, int h, int on);
void gfx_fill(int x, int y, int w, int h, int on);
void gfx_invert(int x, int y, int w, int h);
void gfx_circle(int cx, int cy, int r, int on);
void gfx_disc(int cx, int cy, int r, int on);
void gfx_thick_line(int x0, int y0, int x1, int y1, int t, int on);

/* Texto pequeno 6x8 en cualquier posicion (y = fila de arriba). scale 1 o 2. Devuelve el ancho. */
int  gfx_text(int x, int y, const char *s, int scale, int align);
int  gfx_text_w(const char *s, int scale);

/* Cifras grandes. Devuelve el ancho. Caracteres no soportados se saltan. */
int  gfx_big(int x, int y, const font_big_t *f, const char *s, int align);
int  gfx_big_w(const font_big_t *f, const char *s);

#endif
