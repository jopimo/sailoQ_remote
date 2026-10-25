/*
 * st7571.c - Driver de la pantalla ERC128128FS02-3 (ST7571, 128x128, FSTN positivo).
 *
 * Interfaz SPI de 4 hilos (PS0=0, PS1=1, PS2=0 en la placa): SCK=DB6, SDA=DB7, A0 (0 comando / 1 dato), CS.
 * La pantalla no se puede leer, asi que se dibuja en un "framebuffer" en RAM (gfx.c) y se vuelca entero.
 *
 * El ST7571 es de 4 grises: cada columna de cada pagina (8 filas) ocupa DOS bytes (dos planos de bit).
 *   blanco = 00, gris claro = 01, gris oscuro = 10, negro = 11.
 * Aqui solo se usa blanco/negro: se envia el mismo byte dos veces.
 *
 * Secuencia de arranque tomada del ejemplo de la hoja del ST7571 (Initial_ST7571) y de la hoja del modulo
 * (bias 1/12, duty 1/128). El contraste (EV) se ajusta en el menu y se guarda en la EEPROM.
 */
#include "st7571.h"
#include "periph.h"

static void cs(int v) { HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void a0(int v) { HAL_GPIO_WritePin(LCD_CS_PORT, LCD_A0_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }

static void cmd(const uint8_t *c, uint16_t n)
{
    a0(0); cs(0);
    HAL_SPI_Transmit(&hspi_lcd, (uint8_t *)c, n, 20);
    cs(1);
}

static void cmd1(uint8_t c) { cmd(&c, 1); }

void lcd_contrast(uint8_t ev)
{
    uint8_t c[2] = { 0x81, (uint8_t)(ev & 0x3F) };   /* Set electronic volume (EV 0..63) */
    cmd(c, 2);
}

void lcd_init(uint8_t contrast)
{
    /* Reset por hardware */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);

    static const uint8_t init1[] = {
        0xAE,                               /* pantalla apagada */
        0x38, 0xB4,                         /* Mode set: FR=1011 (85 Hz), BE=01 (eficiencia del elevador nivel 2) */
        (uint8_t)(0xA0 | LCD_ADC_REV),      /* ADC: sentido de las columnas */
        (uint8_t)(0xC0 | (LCD_SHL_REV << 3)),/* SHL: sentido de las filas */
        0x44, (uint8_t)LCD_COM0,            /* fila COM0 inicial */
        0x40, 0x00,                         /* linea inicial de la RAM = 0 */
        0xAB,                               /* arranca el oscilador interno */
        0x67,                               /* elevador DC-DC x8 (ejemplo de Sitronix) */
        0x25,                               /* relacion del regulador (1 + Ra/Rb) = 5 */
        0x57,                               /* bias 1/12 (hoja del modulo) */
        0xF3, 0x04,                         /* sale del ahorro del bias (ejemplo de Sitronix: F3 + parametro) */
        0xA6,                               /* imagen normal (no invertida) */
        0xA4,                               /* muestra la RAM (no "todo encendido") */
    };
    cmd(init1, sizeof init1);
    lcd_contrast(contrast);

    /* Encendido escalonado de la fuente interna: elevador, regulador y seguidor */
    cmd1(0x2C); HAL_Delay(200);
    cmd1(0x2E); HAL_Delay(200);
    cmd1(0x2F); HAL_Delay(200);
    cmd1(0xAF);                             /* pantalla encendida */
}

void lcd_flush(const uint8_t fb[LCD_H / 8][LCD_W])
{
    static uint8_t line[LCD_W * 2];
    for (uint8_t p = 0; p < LCD_H / 8; p++) {
        uint8_t c[3] = { (uint8_t)(0xB0 | p), 0x10, 0x00 };   /* pagina p, columna 0 */
        cmd(c, 3);
        for (int x = 0; x < LCD_W; x++) { line[2 * x] = fb[p][x]; line[2 * x + 1] = fb[p][x]; }
        a0(1); cs(0);
        HAL_SPI_Transmit(&hspi_lcd, line, sizeof line, 50);
        cs(1);
    }
}

void lcd_sleep(void)
{
    cmd1(0xAE);                             /* apagar */
    cmd1(0xA9);                             /* modo ahorro (sleep) */
}

void lcd_wake(void)
{
    cmd1(0xE1);                             /* sale del ahorro */
    HAL_Delay(20);
    cmd1(0xAF);
}
