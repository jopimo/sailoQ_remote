/* Sustituto minimo de la HAL para compilar la interfaz en el PC (test/host). */
#ifndef FAKE_HAL_H
#define FAKE_HAL_H
#include <stdint.h>
typedef struct { int x; } SPI_HandleTypeDef, TIM_HandleTypeDef, ADC_HandleTypeDef, UART_HandleTypeDef, RTC_HandleTypeDef, IWDG_HandleTypeDef;
typedef struct { volatile uint32_t IDR, BSRR; } GPIO_TypeDef;
extern GPIO_TypeDef fake_gpio;
#define GPIOA (&fake_gpio)
#define GPIOB (&fake_gpio)
#define SPI1 0
#define SPI2 0
#define USART1 0
#define GPIO_PIN_0 0x0001u
#define GPIO_PIN_1 0x0002u
#define GPIO_PIN_2 0x0004u
#define GPIO_PIN_3 0x0008u
#define GPIO_PIN_4 0x0010u
#define GPIO_PIN_5 0x0020u
#define GPIO_PIN_6 0x0040u
#define GPIO_PIN_7 0x0080u
#define GPIO_PIN_8 0x0100u
#define GPIO_PIN_9 0x0200u
#define GPIO_PIN_10 0x0400u
#define GPIO_PIN_12 0x1000u
#define GPIO_PIN_13 0x2000u
#define GPIO_PIN_14 0x4000u
#define GPIO_PIN_15 0x8000u
#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define ADC_CHANNEL_3 3
uint32_t HAL_GetTick(void);
#endif
