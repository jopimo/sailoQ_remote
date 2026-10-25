/*
 * sq_port.c - Lo que la libreria de radio sailoQ_radio necesita de esta placa (ver sq_port.h):
 * nRF24 en SPI1 (PA5/PA6/PA7), CSN PA4, CE PB0, IRQ PB1. El SPI y los pines los configura periph.c.
 */
#include "sq_port.h"
#include "periph.h"

void sq_port_spi(const uint8_t *tx, uint8_t *rx, uint8_t n)
{
    HAL_SPI_TransmitReceive(&hspi_nrf, (uint8_t *)tx, rx, n, 10);
}

void     sq_port_csn(int v)          { HAL_GPIO_WritePin(NRF_CSN_PORT, NRF_CSN_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void     sq_port_ce(int v)           { HAL_GPIO_WritePin(NRF_CE_PORT, NRF_CE_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
int      sq_port_irq(void)           { return HAL_GPIO_ReadPin(NRF_IRQ_PORT, NRF_IRQ_PIN) == GPIO_PIN_RESET; }
void     sq_port_delay_us(uint32_t us) { delay_us(us); }
uint32_t sq_port_ms(void)            { return HAL_GetTick(); }

void sq_port_uid(uint32_t w[3])
{
    w[0] = HAL_GetUIDw0(); w[1] = HAL_GetUIDw1(); w[2] = HAL_GetUIDw2();
}
