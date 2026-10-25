/*
 * periph.c - Reloj y perifericos del sailoQ remote (escrito a mano, sin CubeMX).
 *
 *  - SYSCLK = HSI16 (16 MHz), regulador en rango 2 (1,5 V): suficiente y gasta poco.
 *  - LSE 32,768 kHz -> RTC (despertar periodico en Stop para refrescar el watchdog).
 *  - Al salir de Stop el micro vuelve directamente a HSI16 (WAKEUPSTOP_CLK = HSI).
 */
#include <stdarg.h>
#include <stdio.h>
#include "periph.h"

/* Valor de VREFINT medido en fabrica a 3,0 V y 30 C (hoja del STM32L071, apartado "Embedded internal reference voltage") */
#define VREFINT_CAL_ADDR ((const uint16_t *)0x1FF80078u)

#ifndef SQ_DEBUG_UART
#define SQ_DEBUG_UART 1
#endif

SPI_HandleTypeDef  hspi_nrf;
SPI_HandleTypeDef  hspi_lcd;
TIM_HandleTypeDef  htim_bl;
TIM_HandleTypeDef  htim_buz;
ADC_HandleTypeDef  hadc;
UART_HandleTypeDef huart_dbg;
RTC_HandleTypeDef  hrtc;
IWDG_HandleTypeDef hiwdg;
volatile uint8_t   rtc_woke;

void Error_Handler(void)
{
    __disable_irq();
    /* Sin LEDs propios: se deja la retroiluminacion encendida fija como aviso y se espera al watchdog */
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET);
    while (1) { }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);   /* 16 MHz maximo */

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMLOW);              /* cristal de 12,5 pF */

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSEState = RCC_LSE_ON;
    osc.LSIState = RCC_LSI_ON;                                       /* para el IWDG */
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        /* Si el LSE no arranca (cristal mal soldado) seguimos sin el: el RTC usara el LSI */
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
        osc.LSEState = RCC_LSE_OFF;
        if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC | RCC_PERIPHCLK_USART1;
    pclk.RTCClockSelection = __HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) ? RCC_RTCCLKSOURCE_LSE : RCC_RTCCLKSOURCE_LSI;
    pclk.Usart1ClockSelection = RCC_USART1CLKSOURCE_HSI;
    HAL_RCCEx_PeriphCLKConfig(&pclk);

    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_HSI);
}

/* ------------------------------------------------------------------ GPIO */
static void gpio_init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* Pines libres y no usados como analogicos (lo que menos consume) */
    g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
    g.Pin = GPIO_PIN_11 | GPIO_PIN_12;              HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_11;                            HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_13;                            HAL_GPIO_Init(GPIOC, &g);
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;                HAL_GPIO_Init(GPIOH, &g);
    /* Entradas analogicas */
    g.Pin = VBUS_DET_PIN | VBAT_ADC_PIN | LIGHT_ADC_PIN; HAL_GPIO_Init(GPIOA, &g);

    /* Salidas: CS de radio y pantalla en alto, CE del nRF bajo, reset de pantalla bajo, sensor de luz apagado */
    HAL_GPIO_WritePin(NRF_CSN_PORT, NRF_CSN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(NRF_CE_PORT, NRF_CE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LIGHT_EN_PORT, LIGHT_EN_PIN, GPIO_PIN_RESET);
    g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin = NRF_CSN_PIN;                            HAL_GPIO_Init(NRF_CSN_PORT, &g);
    g.Pin = NRF_CE_PIN;                             HAL_GPIO_Init(NRF_CE_PORT, &g);
    g.Pin = LCD_CS_PIN | LCD_A0_PIN | LCD_RST_PIN;  HAL_GPIO_Init(GPIOB, &g);
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = LIGHT_EN_PIN;                           HAL_GPIO_Init(LIGHT_EN_PORT, &g);

    /* IRQ del nRF: entrada con pull-up (se lee por sondeo) */
    g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    g.Pin = NRF_IRQ_PIN;                            HAL_GPIO_Init(NRF_IRQ_PORT, &g);

    /* Teclas: pull-up interno, interrupcion en flanco de bajada (despiertan del modo Stop) */
    g.Mode = GPIO_MODE_IT_FALLING; g.Pull = GPIO_PULLUP;
    g.Pin = KEYS_ALL_PINS;                          HAL_GPIO_Init(KEYS_PORT, &g);
    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 2, 0);  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0); HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/* ------------------------------------------------------------------ SPI */
static void spi_init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate = GPIO_AF0_SPI1;
    g.Pin = NRF_SCK_PIN | NRF_MISO_PIN | NRF_MOSI_PIN;  HAL_GPIO_Init(GPIOA, &g);
    g.Alternate = GPIO_AF0_SPI2;
    g.Pin = LCD_SCK_PIN | LCD_SDA_PIN;                  HAL_GPIO_Init(GPIOB, &g);

    /* nRF24L01: modo 0, MSB primero, 8 MHz (maximo del nRF: 10 MHz) */
    hspi_nrf.Instance = NRF_SPI;
    hspi_nrf.Init.Mode = SPI_MODE_MASTER;
    hspi_nrf.Init.Direction = SPI_DIRECTION_2LINES;
    hspi_nrf.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi_nrf.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi_nrf.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi_nrf.Init.NSS = SPI_NSS_SOFT;
    hspi_nrf.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi_nrf.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi_nrf.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi_nrf.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi_nrf.Init.CRCPolynomial = 7;
    if (HAL_SPI_Init(&hspi_nrf) != HAL_OK) Error_Handler();

    /* ST7571: el dato se lee en el flanco de subida (modo 3 tambien vale; se usa modo 0), 4 MHz, solo transmision */
    hspi_lcd = hspi_nrf;
    hspi_lcd.Instance = LCD_SPI;
    hspi_lcd.Init.Direction = SPI_DIRECTION_2LINES;          /* MISO no esta conectado */
    hspi_lcd.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    if (HAL_SPI_Init(&hspi_lcd) != HAL_OK) Error_Handler();
}

/* ------------------------------------------------------------------ TIM: PWM */
static void tim_init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM21_CLK_ENABLE();

    /* Retroiluminacion: TIM2_CH1 en PA15 (AF5), 1 kHz, 0..1000 */
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = LCD_BL_PIN; g.Alternate = GPIO_AF5_TIM2; HAL_GPIO_Init(LCD_BL_PORT, &g);
    htim_bl.Instance = TIM2;
    htim_bl.Init.Prescaler = 15;                 /* 16 MHz / 16 = 1 MHz */
    htim_bl.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_bl.Init.Period = 999;                   /* 1 kHz */
    htim_bl.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_PWM_Init(&htim_bl) != HAL_OK) Error_Handler();
    oc.OCMode = TIM_OCMODE_PWM1; oc.Pulse = 0; oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    HAL_TIM_PWM_ConfigChannel(&htim_bl, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim_bl, TIM_CHANNEL_1);

    /* Zumbador: TIM21_CH1 en PA2 (AF0), 2,7 kHz (resonancia del MLT-8530). Arranca parado. */
    g.Pin = BUZ_PIN; g.Alternate = GPIO_AF0_TIM21; HAL_GPIO_Init(BUZ_PORT, &g);
    htim_buz.Instance = TIM21;
    htim_buz.Init.Prescaler = 0;
    htim_buz.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_buz.Init.Period = (16000000u / 2700u) - 1u;
    htim_buz.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_PWM_Init(&htim_buz) != HAL_OK) Error_Handler();
    oc.Pulse = 0;
    HAL_TIM_PWM_ConfigChannel(&htim_buz, &oc, TIM_CHANNEL_1);
}

void bl_set(uint8_t percent)
{
    if (percent > 100) percent = 100;
    __HAL_TIM_SET_COMPARE(&htim_bl, TIM_CHANNEL_1, (uint32_t)percent * 10u);
}

/* ------------------------------------------------------------------ ADC */
static void adc_init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc.Instance = ADC1;
    hadc.Init.OversamplingMode = DISABLE;
    hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;   /* 8 MHz */
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;   /* fuentes de alta impedancia (divisor de 1 MOhm) */
    hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc.Init.LowPowerAutoWait = DISABLE;
    hadc.Init.LowPowerFrequencyMode = DISABLE;
    hadc.Init.LowPowerAutoPowerOff = ENABLE;               /* el ADC se apaga solo entre conversiones */
    if (HAL_ADC_Init(&hadc) != HAL_OK) Error_Handler();
    HAL_ADCEx_Calibration_Start(&hadc, ADC_SINGLE_ENDED);
}

uint16_t adc_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef c = {0};
    c.Channel = channel; c.Rank = ADC_RANK_CHANNEL_NUMBER;
    HAL_ADC_ConfigChannel(&hadc, &c);
    uint16_t v = 0;
    if (HAL_ADC_Start(&hadc) == HAL_OK && HAL_ADC_PollForConversion(&hadc, 5) == HAL_OK)
        v = (uint16_t)HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);
    c.Rank = ADC_RANK_NONE;                                /* quitar el canal de la secuencia */
    HAL_ADC_ConfigChannel(&hadc, &c);
    return v;
}

uint16_t adc_vdda_mv(void)
{
    uint16_t raw = adc_read(ADC_CHANNEL_VREFINT);
    if (raw == 0) return 3300;
    return (uint16_t)(3000u * (uint32_t)(*VREFINT_CAL_ADDR) / raw);   /* VREFINT_CAL medido de fabrica a 3,0 V */
}

/* ------------------------------------------------------------------ USART1 depuracion */
static void uart_init(void)
{
#if SQ_DEBUG_UART
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_LOW; g.Alternate = GPIO_AF4_USART1;
    g.Pin = DBG_TX_PIN | DBG_RX_PIN; HAL_GPIO_Init(GPIOA, &g);
    huart_dbg.Instance = DBG_UART;
    huart_dbg.Init.BaudRate = 115200;
    huart_dbg.Init.WordLength = UART_WORDLENGTH_8B;
    huart_dbg.Init.StopBits = UART_STOPBITS_1;
    huart_dbg.Init.Parity = UART_PARITY_NONE;
    huart_dbg.Init.Mode = UART_MODE_TX_RX;
    huart_dbg.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart_dbg.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart_dbg);
#endif
}

void dbg_printf(const char *fmt, ...)
{
#if SQ_DEBUG_UART
    char b[128];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) HAL_UART_Transmit(&huart_dbg, (uint8_t *)b, (uint16_t)(n < (int)sizeof b ? n : (int)sizeof b - 1), 50);
#else
    (void)fmt;
#endif
}

/* ------------------------------------------------------------------ RTC y watchdog */
static void rtc_init(void)
{
    __HAL_RCC_RTC_ENABLE();
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;                 /* 32768 / 128 / 256 = 1 Hz */
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(RTC_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
}

void rtc_wakeup_set(uint16_t seconds)
{
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    if (seconds) HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (uint32_t)seconds - 1u, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *h) { (void)h; rtc_woke = 1; }

static void wdg_init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;   /* LSI ~37 kHz / 256 = 145 Hz */
    hiwdg.Init.Window = IWDG_WINDOW_DISABLE;
    hiwdg.Init.Reload = 2300;                    /* ~16 s */
    HAL_IWDG_Init(&hiwdg);
}

void wdg_kick(void) { HAL_IWDG_Refresh(&hiwdg); }

/* ------------------------------------------------------------------ varios */
void delay_us(uint32_t us)
{
    /* 16 MHz: ~4 ciclos por vuelta -> 4 vueltas por us (aproximado; solo para esperas cortas del nRF24) */
    volatile uint32_t n = us * 4u;
    while (n--) { __NOP(); }
}

void periph_init(void)
{
    gpio_init();
    spi_init();
    tim_init();
    adc_init();
    uart_init();
    rtc_init();
    wdg_init();
}
