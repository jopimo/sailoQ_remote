/* stm32l0xx_it.c - Interrupciones. */
#include "periph.h"

void NMI_Handler(void) { }
void HardFault_Handler(void) { while (1) { } }       /* el watchdog reinicia la placa */
void SVC_Handler(void) { }
void PendSV_Handler(void) { }
void SysTick_Handler(void) { HAL_IncTick(); }

/* Teclas: solo sirven para despertar del modo Stop; la lectura se hace por sondeo en keys.c */
void EXTI2_3_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(KEY_PREV_PIN); }
void EXTI4_15_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY_NEXT_PIN);
    HAL_GPIO_EXTI_IRQHandler(KEY1_PIN); HAL_GPIO_EXTI_IRQHandler(KEY2_PIN);
    HAL_GPIO_EXTI_IRQHandler(KEY3_PIN); HAL_GPIO_EXTI_IRQHandler(KEY4_PIN);
    HAL_GPIO_EXTI_IRQHandler(KEY5_PIN); HAL_GPIO_EXTI_IRQHandler(KEY6_PIN);
}

void RTC_IRQHandler(void) { HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc); }
