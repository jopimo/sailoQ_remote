#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "keys.h"

#define FW_VERSION "0.1"

/* Paginas */
enum { P_DATOS = 0, P_VIENTO, P_PILOTO, P_MOLINETE, P_HELICE, P_AJUSTES, P_COUNT };

void ui_init(uint32_t now);
void ui_boot_screen(int radio_ok);
void ui_key(const kev_t *e, uint32_t now);
void ui_tick(uint32_t now);              /* radio, combinaciones de teclas y redibujado */
int  ui_busy(void);                      /* 1 = no dormir (emparejando o moviendo un actuador) */
void ui_wake(uint32_t now);              /* tras despertar: refresca datos y pantalla */
void ui_battery_low_screen(void);

#endif
