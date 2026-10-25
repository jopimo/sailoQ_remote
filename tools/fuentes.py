#!/usr/bin/env python3
"""Genera src/font_big.c (cifras grandes) a partir de DejaVu Sans Condensed Bold.
Uso:  python3 tools/fuentes.py   (necesita Pillow: pip install pillow)
Los glifos se estrechan un 15 % porque los pixeles de la pantalla son 0,28 x 0,33 mm."""
from PIL import Image, ImageDraw, ImageFont
import os, sys

TTF = '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf'
CHARS = "0123456789.-:° "
XS = 0.85
FUENTES = [('font_med', 22, 16), ('font_big', 35, 26), ('font_huge', 57, 42)]   # nombre, tamano TTF, alto en pixeles

def gen(out, name, size, H):
    f = ImageFont.truetype(TTF, size)
    bb = f.getbbox('0123456789'); top = bb[1]; hgt = bb[3] - bb[1]
    nb = (H + 7) // 8; glyphs = []
    for ch in CHARS:
        if ch == ' ':
            w = max(3, int(size * 0.22)); glyphs.append((w, [[0] * nb for _ in range(w)])); continue
        b = f.getbbox(ch); w0 = max(1, b[2] - b[0]) + 2
        im = Image.new('L', (w0, H), 0); d = ImageDraw.Draw(im)
        d.text((1 - b[0], (H - hgt) // 2 - top), ch, font=f, fill=255)
        w = max(2, round(w0 * XS)); im = im.resize((w, H), Image.LANCZOS).point(lambda v: 255 if v >= 110 else 0).convert('1')
        px = im.load(); cols = []
        for x in range(w):
            col = [0] * nb
            for y in range(H):
                if px[x, y]: col[y // 8] |= 1 << (y % 8)
            cols.append(col)
        glyphs.append((w, cols))
    data = []; idx = []
    for w, cols in glyphs:
        idx.append((w, len(data)))
        for c in cols: data += c
    out.append('static const uint8_t %s_data[] = {' % name)
    for i in range(0, len(data), 16): out.append('    ' + ', '.join('0x%02X' % v for v in data[i:i + 16]) + ',')
    out.append('};')
    out.append('const font_big_t %s = { %d, %d, "0123456789.-:\\xB0 ", {' % (name, H, nb))
    out.append('    ' + ', '.join('{%d, %d}' % x for x in idx))
    out.append('}, %s_data };' % name); out.append('')

if __name__ == '__main__':
    out = ['/* font_big.c - cifras grandes generadas con DejaVu Sans Condensed Bold (script en tools/fuentes.py).',
           ' * Cada glifo: ancho variable, columnas de izquierda a derecha, bit 0 = fila de arriba de cada byte.',
           ' * Los glifos se estrechan un 15 % porque los pixeles de la pantalla son mas altos que anchos (0,28 x 0,33 mm). */',
           '#include "fonts.h"', '']
    for n, s, h in FUENTES: gen(out, n, s, h)
    dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'font_big.c')
    open(dst, 'w').write('\n'.join(out))
    print('escrito', dst)
