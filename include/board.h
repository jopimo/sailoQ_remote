#ifndef BOARD_H
#define BOARD_H
/*
 * board.h - Mapa de pines del sailoQ remote (esquema SCH_sailoQ-remote_v9, PCB v1.1).
 * MCU: STM32L071CBT6 (LQFP48, 128 KB flash, 20 KB RAM, 6 KB EEPROM de datos).
 * Reloj: HSI16 interno (16 MHz) + cristal LSE de 32,768 kHz para el RTC. Sin cristal HSE.
 */
#include "stm32l0xx_hal.h"

/* ---------- Radio nRF24L01 (modulo enchufado por detras, RF1) - SPI1 ---------- */
#define NRF_SPI             SPI1
#define NRF_CSN_PORT        GPIOA
#define NRF_CSN_PIN         GPIO_PIN_4      /* NRF_CSN  */
#define NRF_SCK_PIN         GPIO_PIN_5      /* SPI1_SCK  AF0 */
#define NRF_MISO_PIN        GPIO_PIN_6      /* SPI1_MISO AF0 */
#define NRF_MOSI_PIN        GPIO_PIN_7      /* SPI1_MOSI AF0 */
#define NRF_CE_PORT         GPIOB
#define NRF_CE_PIN          GPIO_PIN_0      /* NRF_CE */
#define NRF_IRQ_PORT        GPIOB
#define NRF_IRQ_PIN         GPIO_PIN_1      /* NRF_IRQ, activa a nivel bajo, EXTI1 */

/* ---------- Pantalla ERC128128FS02-3 (ST7571) - SPI2, 4 hilos, solo escritura ---------- */
#define LCD_SPI             SPI2
#define LCD_CS_PORT         GPIOB
#define LCD_CS_PIN          GPIO_PIN_12     /* LCD_CS (R15 100k a 3V3) */
#define LCD_SCK_PIN         GPIO_PIN_13     /* SPI2_SCK  AF0 -> DB6 de la pantalla */
#define LCD_A0_PIN          GPIO_PIN_14     /* LCD_A0 (GPIO): 0 = comando, 1 = dato */
#define LCD_SDA_PIN         GPIO_PIN_15     /* SPI2_MOSI AF0 -> DB7 de la pantalla */
#define LCD_RST_PORT        GPIOB
#define LCD_RST_PIN         GPIO_PIN_2      /* LCD_RST, activo a nivel bajo */
#define LCD_BL_PORT         GPIOA
#define LCD_BL_PIN          GPIO_PIN_15     /* LCD_BL: TIM2_CH1 AF5, PWM a Q3 (retroiluminacion) */

/* ---------- Teclas (silicona conductiva a GND, pull-up interno) ---------- */
/* Disposicion 4 filas x 2 columnas:
 *   fila 1:  KEY_PREV (SW1)  KEY_NEXT (SW2)
 *   fila 2:  KEY1     (SW3)  KEY2     (SW4)
 *   fila 3:  KEY3     (SW5)  KEY4     (SW6)
 *   fila 4:  KEY5     (SW7)  KEY6     (SW8)                                   */
#define KEYS_PORT           GPIOB
#define KEY_PREV_PIN        GPIO_PIN_3      /* EXTI3 */
#define KEY_NEXT_PIN        GPIO_PIN_4      /* EXTI4 */
#define KEY1_PIN            GPIO_PIN_5      /* EXTI5 */
#define KEY2_PIN            GPIO_PIN_6
#define KEY3_PIN            GPIO_PIN_7
#define KEY4_PIN            GPIO_PIN_8
#define KEY5_PIN            GPIO_PIN_9
#define KEY6_PIN            GPIO_PIN_10     /* EXTI10 */
#define KEYS_ALL_PINS       (KEY_PREV_PIN | KEY_NEXT_PIN | KEY1_PIN | KEY2_PIN | KEY3_PIN | KEY4_PIN | KEY5_PIN | KEY6_PIN)

/* ---------- Zumbador magnetico MLT-8530 (2,7 kHz) via Q5 ---------- */
#define BUZ_PORT            GPIOA
#define BUZ_PIN             GPIO_PIN_2      /* BUZZER: TIM21_CH1 AF0 */

/* ---------- Medidas analogicas (ADC) ---------- */
#define VBUS_DET_PIN        GPIO_PIN_0      /* PA0 = ADC_IN0, VBUS/2 (USB o pogo). Tambien EXTI0 para despertar */
#define VBAT_ADC_PIN        GPIO_PIN_1      /* PA1 = ADC_IN1, VBAT/2 (R9/R10 1M + C7 100n) */
#define LIGHT_ADC_PIN       GPIO_PIN_3      /* PA3 = ADC_IN3, sensor de luz LS1 con R22 47k a GND */
#define ADC_CH_VBUS         ADC_CHANNEL_0
#define ADC_CH_VBAT         ADC_CHANNEL_1
#define ADC_CH_LIGHT        ADC_CHANNEL_3
#define LIGHT_EN_PORT       GPIOA
#define LIGHT_EN_PIN        GPIO_PIN_8      /* alto = alimenta el sensor de luz (solo mientras se mide) */

/* ---------- Depuracion: tira DBG (1 GND, 2 3V3, 3 TX, 4 RX, 5 BOOT0, 6 VBAT) ---------- */
#define DBG_UART            USART1
#define DBG_TX_PIN          GPIO_PIN_9      /* USART1_TX AF4 */
#define DBG_RX_PIN          GPIO_PIN_10     /* USART1_RX AF4 */
/* SWD: PA13 SWDIO / PA14 SWCLK en el conector PROG (1 GND, 2 CLK, 3 DIO, 4 3V3, 5 RST). NO reconfigurar. */
/* NRST tambien va al reed SW9 (iman = reset). LSE en PC14/PC15. */

/* Pines libres: PA11, PA12, PB11, PC13, PH0, PH1 */

#endif
