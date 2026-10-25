#ifndef ST7571_H
#define ST7571_H
#include <stdint.h>

/* Pantalla 128x128 (ERC128128FS02-3, controlador ST7571 en el cristal). Solo escritura (SPI de 4 hilos). */
#define LCD_W 128
#define LCD_H 128

/* Orientacion: hay que confirmarla con la pantalla real (tarea F2 de CLAUDE.md).
 * Con la FPC saliendo por abajo y el texto derecho deberia ser ADC=1 y SHL=1 (como el codigo de ejemplo de Sitronix). */
#ifndef LCD_ADC_REV
#define LCD_ADC_REV 1       /* 1 = columnas SEG127..SEG0 (espejo horizontal) */
#endif
#ifndef LCD_SHL_REV
#define LCD_SHL_REV 1       /* 1 = filas COM127..COM0 (espejo vertical) */
#endif
#ifndef LCD_COM0
#define LCD_COM0    0       /* fila inicial; si la imagen sale desplazada, probar con otros valores */
#endif

void lcd_init(uint8_t contrast);          /* contraste = EV del ST7571, 0..63 (valor tipico ~35) */
void lcd_contrast(uint8_t ev);
void lcd_flush(const uint8_t fb[LCD_H / 8][LCD_W]);   /* vuelca la imagen entera (unos 10 ms) */
void lcd_sleep(void);                     /* apaga la pantalla y entra en ahorro (unos uA) */
void lcd_wake(void);

#endif
