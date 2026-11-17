/* test_teclas.c - Prueba en el PC de keys.c (antirrebote, eventos y la tecla que despierta).
 * Se ejecuta desde probar.sh. Los pines de las teclas se simulan con fake_gpio.IDR (1 = suelta). */
#include <stdio.h>
#include "keys.h"
#include "board.h"

GPIO_TypeDef fake_gpio;
uint32_t HAL_GetTick(void) { return 0; }

static int checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FALLO linea %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const uint16_t pin[K_COUNT] = { KEY_PREV_PIN, KEY_NEXT_PIN, KEY1_PIN, KEY2_PIN, KEY3_PIN, KEY4_PIN, KEY5_PIN, KEY6_PIN };
static uint32_t t;

static void set(uint8_t mask) { uint32_t idr = 0xFFFF; for (int k = 0; k < K_COUNT; k++) if (mask & KB(k)) idr &= ~(uint32_t)pin[k]; fake_gpio.IDR = idr; }
static void run(uint32_t ms) { for (uint32_t i = 0; i < ms; i += 10) { t += 10; keys_scan(t); } }
static int count(kev_type_t type, uint8_t key) { kev_t e; int n = 0; while (keys_get(&e)) if (e.type == type && e.key == key) n++; return n; }

int main(void)
{
    /* Pulsacion normal: PRESS, LONG y RELEASE */
    set(0); keys_init();
    set(KB(K_1)); run(50);
    CHECK(keys_down() == KB(K_1), "K1 pulsada no aparece en keys_down");
    run(700);
    set(0); run(50);
    kev_t e; int press = 0, lng = 0, rel = 0;
    while (keys_get(&e)) { if (e.key != K_1) continue; press += e.type == KEV_PRESS; lng += e.type == KEV_LONG; rel += e.type == KEV_RELEASE; }
    CHECK(press == 1 && lng == 1 && rel == 1, "K1: PRESS %d, LONG %d, RELEASE %d (esperado 1, 1, 1)", press, lng, rel);

    /* Rebote de 20 ms: no cuenta */
    set(KB(K_2)); run(20); set(0); run(50);
    CHECK(count(KEV_PRESS, K_2) == 0, "un rebote de 20 ms dio PRESS");

    /* Despertar con K1 pulsada (molinete): no puede aparecer ni dar eventos hasta soltarla */
    set(KB(K_1));
    keys_ignore_held();
    run(1000);
    CHECK(keys_down() == 0, "la tecla del despertar aparece en keys_down (moveria el molinete)");
    CHECK(keys_any() == KB(K_1), "keys_any deberia ver la tecla del despertar");
    set(KB(K_1) | KB(K_2)); run(50);
    CHECK(keys_down() == KB(K_2), "otra tecla pulsada despues si debe contar");
    set(KB(K_2)); run(50);
    CHECK(count(KEV_RELEASE, K_1) == 0, "la tecla del despertar dio RELEASE al soltarla");
    set(0); run(50);
    set(KB(K_1)); run(50);
    CHECK(keys_down() == KB(K_1), "tras soltarla, la siguiente pulsacion de K1 debe contar");
    CHECK(count(KEV_PRESS, K_1) == 1, "tras soltarla, la siguiente pulsacion de K1 debe dar PRESS");
    set(0); run(50);

    /* Tecla pulsada durante el arranque: igual */
    set(KB(K_6)); keys_init(); run(1000);
    CHECK(keys_down() == 0 && count(KEV_LONG, K_6) == 0, "tecla pulsada al arrancar no debe actuar");

    printf("teclas: %d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
