#ifndef POWER_H
#define POWER_H
#include <stdint.h>

#define VBAT_LOW_MV     3400    /* aviso: icono de bateria parpadeando */
#define VBAT_CUT_MV     3300    /* corte: mensaje y a dormir (la proteccion DW01A corta a ~2,4 V) */

void     power_measure(void);           /* mide bateria, USB y luz (llamar cada ~2 s despierto) */
uint16_t power_vbat_mv(void);
uint8_t  power_vbat_pct(void);
int      power_charging(void);          /* 1 = hay 5 V por USB o pogo */
uint16_t power_light(void);             /* 0..4095: luz ambiente (sensor LS1) */

/* Duerme en modo Stop (unos uA) hasta que se pulse una tecla.
 * Despierta cada 8 s con el RTC solo para refrescar el watchdog y vuelve a dormir. */
void     power_sleep_until_key(void);

#endif
