/*
 * keys.c - Lectura de las 8 teclas de silicona conductiva (pull-up interno, pulsada = 0).
 * Antirrebote de 30 ms (3 lecturas iguales a 10 ms) y cola de eventos: PRESS, LONG (a 600 ms) y RELEASE.
 */
#include "keys.h"
#include "board.h"

static const uint16_t pins[K_COUNT] = { KEY_PREV_PIN, KEY_NEXT_PIN, KEY1_PIN, KEY2_PIN, KEY3_PIN, KEY4_PIN, KEY5_PIN, KEY6_PIN };

static uint8_t  stable, last, same;
static uint32_t t_down[K_COUNT];
static uint8_t  long_sent;
static kev_t    q[16];
static uint8_t  qh, qt;

static void push(kev_type_t t, uint8_t k, uint16_t ms)
{
    uint8_t n = (uint8_t)((qh + 1) & 15);
    if (n == qt) return;                       /* cola llena: se pierde el evento */
    q[qh].type = t; q[qh].key = k; q[qh].ms = ms; qh = n;
}

uint8_t keys_raw(void)
{
    uint32_t idr = KEYS_PORT->IDR;
    uint8_t m = 0;
    for (int k = 0; k < K_COUNT; k++) if (!(idr & pins[k])) m |= (uint8_t)KB(k);
    return m;
}

void keys_init(void) { stable = last = keys_raw(); same = 0; qh = qt = 0; long_sent = stable; }
void keys_flush(void) { qh = qt = 0; long_sent = stable; }
uint8_t keys_down(void) { return stable; }

uint16_t keys_held_ms(uint8_t key, uint32_t now)
{
    if (!(stable & KB(key))) return 0;
    uint32_t d = now - t_down[key];
    return d > 65535u ? 65535u : (uint16_t)d;
}

void keys_scan(uint32_t now)
{
    uint8_t r = keys_raw();
    if (r != last) { last = r; same = 0; }
    else if (same < 3) same++;
    if (same == 2 && r != stable) {            /* 3 lecturas iguales */
        uint8_t ch = (uint8_t)(r ^ stable);
        for (uint8_t k = 0; k < K_COUNT; k++) {
            if (!(ch & KB(k))) continue;
            if (r & KB(k)) { t_down[k] = now; long_sent &= (uint8_t)~KB(k); push(KEV_PRESS, k, 0); }
            else { uint32_t d = now - t_down[k]; push(KEV_RELEASE, k, d > 65535u ? 65535u : (uint16_t)d); }
        }
        stable = r;
    }
    for (uint8_t k = 0; k < K_COUNT; k++)
        if ((stable & KB(k)) && !(long_sent & KB(k)) && now - t_down[k] >= KEY_LONG_MS) {
            long_sent |= (uint8_t)KB(k);
            push(KEV_LONG, k, KEY_LONG_MS);
        }
}

int keys_get(kev_t *ev)
{
    if (qt == qh) return 0;
    *ev = q[qt]; qt = (uint8_t)((qt + 1) & 15);
    return 1;
}
