#ifndef PERIPH_H
#define PERIPH_H
#include <stdint.h>
#include "board.h"

extern SPI_HandleTypeDef  hspi_nrf;     /* SPI1 -> nRF24L01 */
extern SPI_HandleTypeDef  hspi_lcd;     /* SPI2 -> ST7571 */
extern TIM_HandleTypeDef  htim_bl;      /* TIM2  CH1 -> retroiluminacion (PWM 1 kHz) */
extern TIM_HandleTypeDef  htim_buz;     /* TIM21 CH1 -> zumbador (2,7 kHz) */
extern ADC_HandleTypeDef  hadc;
extern UART_HandleTypeDef huart_dbg;    /* USART1 en la tira DBG */
extern RTC_HandleTypeDef  hrtc;
extern IWDG_HandleTypeDef hiwdg;

void SystemClock_Config(void);
void periph_init(void);
void Error_Handler(void);

/* Retroiluminacion 0..100 % */
void bl_set(uint8_t percent);

/* ADC: lectura de un canal (12 bits) y tension de alimentacion real (mV) con VREFINT */
uint16_t adc_read(uint32_t channel);
uint16_t adc_vdda_mv(void);

/* RTC: despertador periodico (s). 0 = desactivado */
void rtc_wakeup_set(uint16_t seconds);
extern volatile uint8_t rtc_woke;

/* Watchdog (IWDG ~16 s). Hay que refrescarlo al menos cada 8 s, tambien dormido (lo hace el RTC). */
void wdg_kick(void);

/* Depuracion por la tira DBG (USART1 115200 8N1). Sin efecto si SQ_DEBUG_UART=0 */
void dbg_printf(const char *fmt, ...);

/* Espera activa corta en microsegundos (para el nRF24) */
void delay_us(uint32_t us);

#endif
