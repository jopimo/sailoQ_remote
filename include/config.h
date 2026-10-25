#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include "sq_mando.h"

/*
 * Ajustes del mando guardados en la EEPROM de datos del STM32L071 (0x08080000).
 * No se pierden al cambiar la pila ni al reprogramar (salvo borrado total con el ST-Link).
 */
typedef sq_peer_t peer_t;       /* receptor emparejado (canal, direccion, nombre...): lo define la libreria de radio */

typedef struct {
    uint32_t magic;             /* CFG_MAGIC */
    peer_t   base;              /* lite / pro: datos de navegacion y piloto */
    peer_t   act;               /* receptor de molinete + helice de proa */
    uint16_t apagado_s;         /* segundos sin tocar teclas hasta apagar la pantalla */
    uint8_t  brillo;            /* retroiluminacion 0..100 %; 255 = automatico con el sensor de luz */
    uint8_t  pitido;            /* 1 = pitido al pulsar */
    uint8_t  contraste;         /* EV del ST7571 (0..63) */
    uint8_t  datos_campos;      /* pagina DATOS: 2 (bidata) o 3 (tridata) campos */
    uint8_t  reserved[2];
    uint32_t crc;
} cfg_t;

#define CFG_MAGIC 0x52454D31u   /* "REM1" */

_Static_assert(sizeof(cfg_t) % 4 == 0, "cfg_t debe ocupar palabras enteras");

extern cfg_t cfg;

void cfg_defaults(void);
int  cfg_load(void);            /* 0 = OK, -1 = no habia datos validos (se cargan valores por defecto) */
int  cfg_save(void);            /* 0 = OK */

#endif
