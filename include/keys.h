#ifndef KEYS_H
#define KEYS_H
#include <stdint.h>

/* Indices de tecla (ver board.h para la disposicion fisica) */
enum { K_PREV = 0, K_NEXT, K_1, K_2, K_3, K_4, K_5, K_6, K_COUNT };
#define KB(k) (1u << (k))

typedef enum { KEV_NONE = 0, KEV_PRESS, KEV_RELEASE, KEV_LONG } kev_type_t;
typedef struct { kev_type_t type; uint8_t key; uint16_t ms; } kev_t;   /* ms = tiempo pulsada (en RELEASE) */

#define KEY_LONG_MS 600

void     keys_init(void);
void     keys_scan(uint32_t now_ms);      /* llamar cada 10 ms: antirrebote y eventos */
int      keys_get(kev_t *ev);             /* 1 si habia un evento pendiente */
uint8_t  keys_down(void);                 /* mascara de teclas pulsadas ahora (ya filtrada) */
uint16_t keys_held_ms(uint8_t key, uint32_t now_ms);
uint8_t  keys_raw(void);                  /* lectura directa de los pines (1 = pulsada) */
void     keys_flush(void);

#endif
