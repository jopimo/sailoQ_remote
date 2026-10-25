/*
 * main.c - sailoQ remote: mando a distancia con pantalla para la gama sailoQ.
 *
 * Funcionamiento general:
 *   - Despierto: lee teclas cada 10 ms, habla por radio con la base / el actuador y refresca la pantalla.
 *   - Tras cfg.apagado_s sin tocar ninguna tecla: apaga pantalla, retroiluminacion y radio y duerme en
 *     modo Stop (unos uA). Una tecla lo despierta; esa primera pulsacion solo despierta (no actua).
 *   - Bateria por debajo de VBAT_CUT_MV: aviso y a dormir.
 */
#include "periph.h"
#include "config.h"
#include "gfx.h"
#include "keys.h"
#include "radio.h"
#include "power.h"
#include "buzzer.h"
#include "ui.h"

static uint32_t t_key, t_scan, t_meas;
static uint8_t  awake_ignore;           /* 1 = ignorar teclas hasta soltarlas todas (la que nos desperto) */
static uint8_t  low_count;

static void backlight_update(uint32_t now)
{
    uint8_t b = cfg.brillo;
    if (b == 255) {
        /* Automatico: con mucha luz la pantalla transflectiva se ve sin retroiluminacion */
        uint16_t l = power_light();
        int v = 80 - (int)l * 80 / 2000;
        b = (uint8_t)(v < 0 ? 0 : v);
    }
    /* Los ultimos 10 s antes de apagar, la retroiluminacion baja a un tercio como aviso */
    if (cfg.apagado_s > 15 && now - t_key > (uint32_t)(cfg.apagado_s - 10) * 1000u) b /= 3;
    bl_set(b);
}

static void go_sleep(void)
{
    bl_set(0);
    lcd_sleep();
    radio_sleep();
    dbg_printf("dormir\r\n");
    power_sleep_until_key();
    /* despierto */
    dbg_printf("despierto\r\n");
    lcd_wake();
    radio_wake();
    awake_ignore = 1;
    t_key = HAL_GetTick();
    power_measure();
    ui_wake(t_key);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    periph_init();
    dbg_printf("\r\nsailoQ remote v%s\r\n", FW_VERSION);

    if (cfg_load() != 0) dbg_printf("EEPROM vacia: valores por defecto\r\n");
    keys_init();
    power_measure();
    lcd_init(cfg.contraste);
    bl_set(cfg.brillo == 255 ? 50 : cfg.brillo);

    int rok = (radio_init() == 0);
    dbg_printf("radio %s, id %08lX, bateria %u mV\r\n", rok ? "OK" : "NO RESPONDE", (unsigned long)radio_remote_id(), power_vbat_mv());
    ui_boot_screen(rok);
    if (cfg.pitido) buz_beep(rok ? 60 : 400);
    for (uint32_t t0 = HAL_GetTick(); HAL_GetTick() - t0 < 1500; ) { buz_tick(HAL_GetTick()); wdg_kick(); }
    radio_wake();

    ui_init(HAL_GetTick());
    t_key = HAL_GetTick();
    awake_ignore = keys_down() ? 1 : 0;

    for (;;) {
        uint32_t now = HAL_GetTick();
        wdg_kick();
        buz_tick(now);

        if (now - t_scan >= 10) {
            t_scan = now;
            keys_scan(now);
            kev_t e;
            while (keys_get(&e)) {
                t_key = now;
                if (awake_ignore) continue;
                ui_key(&e, now);
            }
            if (awake_ignore && !keys_down()) { awake_ignore = 0; keys_flush(); }
            if (keys_down()) t_key = now;
        }

        if (now - t_meas >= 2000) {
            t_meas = now;
            power_measure();
            backlight_update(now);
            if (power_vbat_mv() < VBAT_CUT_MV && !power_charging()) {
                if (++low_count >= 3) {
                    ui_battery_low_screen();
                    bl_set(30);
                    buz_beep(500);
                    for (uint32_t t0 = HAL_GetTick(); HAL_GetTick() - t0 < 3000; ) { buz_tick(HAL_GetTick()); wdg_kick(); }
                    low_count = 0;
                    go_sleep();
                    continue;
                }
            } else low_count = 0;
        }

        ui_tick(now);

        if (!ui_busy() && now - t_key > (uint32_t)cfg.apagado_s * 1000u) {
            go_sleep();
            continue;
        }
        __WFI();                                 /* espera al siguiente SysTick (1 ms): ahorra algo de consumo */
    }
}
