#ifndef BUZZER_H
#define BUZZER_H
#include <stdint.h>
void buz_beep(uint16_t ms);         /* pitido no bloqueante */
void buz_tick(uint32_t now_ms);     /* llamar en el bucle principal */
#endif
