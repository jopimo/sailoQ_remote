/*
 * buzzer.c - Zumbador magnetico MLT-8530 a 2,7 kHz con TIM21_CH1 (PA2). Ciclo de trabajo 50 %.
 */
#include "buzzer.h"
#include "periph.h"

static uint32_t t_end;
static uint8_t  on;

void buz_beep(uint16_t ms)
{
    __HAL_TIM_SET_COMPARE(&htim_buz, TIM_CHANNEL_1, (htim_buz.Init.Period + 1u) / 2u);
    if (!on) HAL_TIM_PWM_Start(&htim_buz, TIM_CHANNEL_1);
    on = 1;
    t_end = HAL_GetTick() + ms;
}

void buz_tick(uint32_t now)
{
    if (on && (int32_t)(now - t_end) >= 0) {
        HAL_TIM_PWM_Stop(&htim_buz, TIM_CHANNEL_1);   /* la salida queda baja: Q5 cortado */
        on = 0;
    }
}
