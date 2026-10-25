#!/bin/sh
# Compila la interfaz del mando en el PC y genera las pantallas en test/host/out/*.pbm (y PNG si hay Python con Pillow).
set -e
cd "$(dirname "$0")"
mkdir -p out
gcc -std=c11 -Wall -Wextra -O1 -DHOST -Ifake -I../../include -I${SQ_RADIO:-../../../sailoQ_radio}/include sim.c ../../src/ui.c ../../src/gfx.c ../../src/font6x8.c ../../src/font_big.c -o sim
./sim
python3 - <<'PY' 2>/dev/null || true
import glob
from PIL import Image
for f in sorted(glob.glob('out/*.pbm')):
    im = Image.open(f).convert('L').point(lambda v: 40 if v < 128 else 222)
    im.resize((128 * 4, int(128 * 4 * 0.33 / 0.28))).save(f[:-4] + '.png')
PY
