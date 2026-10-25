/*
 * config.c - Ajustes en la EEPROM de datos del STM32L071 (escritura palabra a palabra con la HAL).
 */
#include <stddef.h>
#include <string.h>
#include "config.h"
#include "stm32l0xx_hal.h"

#define CFG_ADDR  DATA_EEPROM_BASE        /* 0x08080000 */

cfg_t cfg;

static uint32_t sum(const cfg_t *c)
{
    const uint8_t *b = (const uint8_t *)c;
    uint32_t s = 0x811C9DC5u;                       /* FNV-1a */
    for (size_t i = 0; i < offsetof(cfg_t, crc); i++) { s ^= b[i]; s *= 16777619u; }
    return s;
}

void cfg_defaults(void)
{
    memset(&cfg, 0, sizeof cfg);
    cfg.apagado_s = 60;
    cfg.brillo = 60;
    cfg.pitido = 1;
    cfg.contraste = 35;
    cfg.datos_campos = 2;
}

int cfg_load(void)
{
    memcpy(&cfg, (const void *)CFG_ADDR, sizeof cfg);
    if (cfg.magic != CFG_MAGIC || cfg.crc != sum(&cfg)) { cfg_defaults(); return -1; }
    return 0;
}

int cfg_save(void)
{
    cfg.magic = CFG_MAGIC;
    cfg.crc = sum(&cfg);
    const uint32_t *w = (const uint32_t *)&cfg;
    int r = 0;
    HAL_FLASHEx_DATAEEPROM_Unlock();
    for (unsigned i = 0; i < sizeof cfg / 4; i++) {
        uint32_t a = CFG_ADDR + i * 4u;
        if (*(const uint32_t *)a == w[i]) continue;          /* no gastar ciclos de escritura */
        if (HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, a, w[i]) != HAL_OK) { r = -1; break; }
    }
    HAL_FLASHEx_DATAEEPROM_Lock();
    return r;
}
