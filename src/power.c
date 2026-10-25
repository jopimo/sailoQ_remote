/*
 * power.c - Bateria, carga, sensor de luz y modo Stop.
 */
#include "power.h"
#include "periph.h"
#include "keys.h"

static uint16_t vbat_mv = 3800, vbus_mv, light;
static uint8_t  first = 1;

void power_measure(void)
{
    uint16_t vdda = adc_vdda_mv();
    uint32_t vb = (uint32_t)adc_read(ADC_CH_VBAT) * vdda * 2u / 4095u;    /* divisor 1M/1M */
    uint32_t vu = (uint32_t)adc_read(ADC_CH_VBUS) * vdda * 2u / 4095u;    /* divisor 100k/100k */
    if (first) { vbat_mv = (uint16_t)vb; first = 0; }
    else vbat_mv = (uint16_t)((vbat_mv * 3u + vb) / 4u);                  /* filtro suave */
    vbus_mv = (uint16_t)vu;

    HAL_GPIO_WritePin(LIGHT_EN_PORT, LIGHT_EN_PIN, GPIO_PIN_SET);         /* alimenta el sensor solo al medir */
    HAL_Delay(2);
    light = adc_read(ADC_CH_LIGHT);
    HAL_GPIO_WritePin(LIGHT_EN_PORT, LIGHT_EN_PIN, GPIO_PIN_RESET);
}

uint16_t power_vbat_mv(void) { return vbat_mv; }
int      power_charging(void) { return vbus_mv > 4000; }
uint16_t power_light(void) { return light; }

uint8_t power_vbat_pct(void)
{
    /* Curva aproximada de una 18650 en reposo */
    static const uint16_t mv[]  = { 3300, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4180 };
    static const uint8_t  pct[] = {    0,   10,   20,   40,   55,   70,   80,   92,  100 };
    if (vbat_mv <= mv[0]) return 0;
    for (int i = 1; i < 9; i++)
        if (vbat_mv <= mv[i]) return (uint8_t)(pct[i - 1] + (uint32_t)(pct[i] - pct[i - 1]) * (vbat_mv - mv[i - 1]) / (mv[i] - mv[i - 1]));
    return 100;
}

void power_sleep_until_key(void)
{
    rtc_wakeup_set(8);
    for (;;) {
        wdg_kick();
        rtc_woke = 0;
        __HAL_GPIO_EXTI_CLEAR_IT(KEYS_ALL_PINS);
        if (keys_raw()) break;                     /* ya hay una tecla pulsada */
        HAL_SuspendTick();
        HAL_PWREx_EnableUltraLowPower();           /* VREFINT apagado en Stop */
        HAL_PWREx_EnableFastWakeUp();
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        HAL_ResumeTick();                          /* sigue con HSI16 (WAKEUPSTOP_CLK = HSI) */
        if (!rtc_woke) break;                      /* lo desperto una tecla */
    }
    rtc_wakeup_set(0);                             /* despierto: el bucle principal refresca el watchdog */
}
