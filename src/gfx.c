/*
 * gfx.c - Dibujo sencillo en un framebuffer monocromo de 128x128 (formato de paginas del ST7571).
 */
#include <stdlib.h>
#include <string.h>
#include "gfx.h"

uint8_t gfx_fb[LCD_H / 8][LCD_W];

void gfx_clear(void) { memset(gfx_fb, 0, sizeof gfx_fb); }
void gfx_show(void)  { lcd_flush((const uint8_t (*)[LCD_W])gfx_fb); }

void gfx_pixel(int x, int y, int on)
{
    if ((unsigned)x >= LCD_W || (unsigned)y >= LCD_H) return;
    uint8_t m = (uint8_t)(1u << (y & 7));
    if (on) gfx_fb[y >> 3][x] |= m; else gfx_fb[y >> 3][x] &= (uint8_t)~m;
}

void gfx_hline(int x0, int x1, int y, int on) { if (x0 > x1) { int t = x0; x0 = x1; x1 = t; } for (int x = x0; x <= x1; x++) gfx_pixel(x, y, on); }
void gfx_vline(int x, int y0, int y1, int on) { if (y0 > y1) { int t = y0; y0 = y1; y1 = t; } for (int y = y0; y <= y1; y++) gfx_pixel(x, y, on); }

void gfx_line(int x0, int y0, int x1, int y1, int on)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_thick_line(int x0, int y0, int x1, int y1, int t, int on)
{
    for (int i = -(t / 2); i <= (t - 1) / 2; i++)
        for (int j = -(t / 2); j <= (t - 1) / 2; j++)
            gfx_line(x0 + i, y0 + j, x1 + i, y1 + j, on);
}

void gfx_rect(int x, int y, int w, int h, int on)
{
    gfx_hline(x, x + w - 1, y, on); gfx_hline(x, x + w - 1, y + h - 1, on);
    gfx_vline(x, y, y + h - 1, on); gfx_vline(x + w - 1, y, y + h - 1, on);
}

void gfx_fill(int x, int y, int w, int h, int on)
{
    for (int j = y; j < y + h; j++) gfx_hline(x, x + w - 1, j, on);
}

void gfx_invert(int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if ((unsigned)i < LCD_W && (unsigned)j < LCD_H) gfx_fb[j >> 3][i] ^= (uint8_t)(1u << (j & 7));
}

void gfx_circle(int cx, int cy, int r, int on)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_pixel(cx + x, cy + y, on); gfx_pixel(cx + y, cy + x, on);
        gfx_pixel(cx - y, cy + x, on); gfx_pixel(cx - x, cy + y, on);
        gfx_pixel(cx - x, cy - y, on); gfx_pixel(cx - y, cy - x, on);
        gfx_pixel(cx + y, cy - x, on); gfx_pixel(cx + x, cy - y, on);
        y++;
        if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
}

void gfx_disc(int cx, int cy, int r, int on)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) gfx_pixel(cx + x, cy + y, on);
}

/* ------------------------------------------------------------------ texto 6x8 */
int gfx_text_w(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }

int gfx_text(int x, int y, const char *s, int scale, int align)
{
    int w = gfx_text_w(s, scale);
    if (align == ALIGN_CENTER) x -= w / 2; else if (align == ALIGN_RIGHT) x -= w;
    for (; *s; s++) {
        uint8_t c = (uint8_t)*s;
        if (c < 32 || c > 127) c = '?';
        const uint8_t *g = font6x8[c - 32];
        for (int i = 0; i < 6; i++)
            for (int b = 0; b < 8; b++)
                if (g[i] >> b & 1) {
                    if (scale == 1) gfx_pixel(x + i, y + b, 1);
                    else gfx_fill(x + i * scale, y + b * scale, scale, scale, 1);
                }
        x += 6 * scale;
    }
    return w;
}

/* ------------------------------------------------------------------ cifras grandes */
static int glyph_idx(const font_big_t *f, char c)
{
    if ((uint8_t)c == 0xB0) c = (char)0xB0;
    for (int i = 0; f->chars[i]; i++) if (f->chars[i] == c) return i;
    return -1;
}

int gfx_big_w(const font_big_t *f, const char *s)
{
    int w = 0;
    for (; *s; s++) { int i = glyph_idx(f, *s); if (i >= 0) w += f->g[i].w; }
    return w;
}

int gfx_big(int x, int y, const font_big_t *f, const char *s, int align)
{
    int w = gfx_big_w(f, s);
    if (align == ALIGN_CENTER) x -= w / 2; else if (align == ALIGN_RIGHT) x -= w;
    for (; *s; s++) {
        int i = glyph_idx(f, *s);
        if (i < 0) continue;
        const uint8_t *d = f->data + f->g[i].off;
        for (int cx = 0; cx < f->g[i].w; cx++, d += f->nb)
            for (int yy = 0; yy < f->h; yy++)
                if (d[yy >> 3] >> (yy & 7) & 1) gfx_pixel(x + cx, y + yy, 1);
        x += f->g[i].w;
    }
    return w;
}
