/* sim.c - Dibuja las pantallas del mando en el PC (PNG en test/host/out) para revisar la interfaz.
 * Compilar y ejecutar: sh test/host/probar.sh */
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "ui.h"
#include "radio.h"
#include "config.h"
#include "power.h"
#include "periph.h"

GPIO_TypeDef fake_gpio;
static uint32_t tick = 100000;
uint32_t HAL_GetTick(void) { return tick; }

/* ---- sustitutos del hardware ---- */
pkt_nav_t nav; pkt_act_status_t act; peer_state_t st_base, st_act;
cfg_t cfg;
static int nav_alive = 1;
int  radio_ok(void) { return 1; }
uint32_t radio_remote_id(void) { return 0x1234ABCD; }
void radio_wake(void) {} void radio_sleep(void) {}
int  radio_poll_base(uint8_t p, uint32_t now) { (void)p; if (nav_alive) { st_base.last_ok_ms = now; st_base.link = 90; } return 0; }
int  radio_pilot_key(uint8_t k, uint32_t now) { (void)k; (void)now; return 0; }
int  radio_act_hold(uint8_t f, uint32_t now) { (void)f; st_act.last_ok_ms = now; st_act.link = 70; return 0; }
int  radio_act_poll(uint32_t now) { st_act.last_ok_ms = now; st_act.link = 70; return 0; }
void radio_pair_start(uint8_t d) { (void)d; }
int  radio_pair_step(void) { return 0; }
int  radio_alive(const peer_state_t *p, uint32_t now, uint32_t ms) { return p->last_ok_ms && now - p->last_ok_ms < ms; }
int  cfg_save(void) { return 0; }
void cfg_defaults(void) { memset(&cfg, 0, sizeof cfg); cfg.apagado_s = 60; cfg.brillo = 60; cfg.pitido = 1; cfg.contraste = 35; cfg.datos_campos = 2; }
uint16_t power_vbat_mv(void) { return 3870; }
uint8_t  power_vbat_pct(void) { return 62; }
int      power_charging(void) { return 0; }
void bl_set(uint8_t p) { (void)p; }
void lcd_contrast(uint8_t e) { (void)e; }
void buz_beep(uint16_t ms) { (void)ms; }
uint8_t keys_down_mask;
uint8_t keys_down(void) { return keys_down_mask; }
uint8_t keys_any(void) { return keys_down_mask; }
uint16_t keys_held_ms(uint8_t k, uint32_t now) { (void)k; (void)now; return 0; }

static char last_name[64];
void lcd_flush(const uint8_t fb[LCD_H / 8][LCD_W])
{
    char fn[96]; snprintf(fn, sizeof fn, "out/%s.pbm", last_name);
    FILE *f = fopen(fn, "w"); if (!f) return;
    fprintf(f, "P1\n128 128\n");
    for (int y = 0; y < 128; y++) { for (int x = 0; x < 128; x++) fprintf(f, "%d ", fb[y >> 3][x] >> (y & 7) & 1); fprintf(f, "\n"); }
    fclose(f);
}

static void shot(const char *name, int page_key_presses)
{
    (void)page_key_presses;
    strcpy(last_name, name);
    tick += 1000;
    ui_tick(tick);
}

static void key(uint8_t k, kev_type_t t) { kev_t e = { t, k, 100 }; ui_key(&e, tick); }

static int checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FALLO linea %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

extern void ui_test_fmt(char *b, int32_t v, int dec);
extern int  ui_test_next_mode(int cur, int wind);

static void check_logic(void)
{
    static const struct { int32_t v; int dec; const char *s; } f[] = {
        { 0, 1, "0.0" }, { 5, 1, "0.5" }, { 64, 1, "6.4" }, { 1234, 1, "123.4" }, { -5, 1, "-0.5" },
        { -42, 0, "-42" }, { 7, 0, "7" }, { 0, 0, "0" },
    };
    char b[16];
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        ui_test_fmt(b, f[i].v, f[i].dec);
        CHECK(!strcmp(b, f[i].s), "fmt_fixed(%ld, %d) = \"%s\" (esperado \"%s\")", (long)f[i].v, f[i].dec, b, f[i].s);
    }
    /* Circulo de modos de K6 mantenida */
    CHECK(ui_test_next_mode(PILOT_STBY, 1) == PILOT_AUTO, "STANDBY -> AUTO");
    CHECK(ui_test_next_mode(PILOT_AUTO, 1) == PILOT_WIND, "AUTO con viento -> VIENTO");
    CHECK(ui_test_next_mode(PILOT_AUTO, 0) == -1, "AUTO sin viento -> nada");
    CHECK(ui_test_next_mode(PILOT_WIND, 1) == PILOT_AUTO, "VIENTO -> AUTO");
    CHECK(ui_test_next_mode(PILOT_TRACK, 1) == PILOT_AUTO, "TRACK -> AUTO");
}

int main(void)
{
    check_logic();
    cfg_defaults();
    strcpy(cfg.base.name, "LITE"); cfg.base.paired = 1;
    strcpy(cfg.act.name, "MOLINETE"); cfg.act.paired = 1;
    nav.valid = NV_SOG | NV_STW | NV_DEPTH | NV_AWA | NV_AWS | NV_TWS | NV_HDG | NV_PILOT;
    nav.sog = 640; nav.stw = 652; nav.depth = 128; nav.awa = -42; nav.aws = 146; nav.tws = 121;
    nav.hdg = 2468; nav.pilot_mode = PILOT_AUTO; nav.pilot_target = 245;
    act.type = PKT_ACT_STATUS; act.flags = AF_CHAIN; act.chain_dm = 324; act.max_s = 30;

    strcpy(last_name, "00_arranque"); ui_boot_screen(1); gfx_show();
    ui_init(tick);
    shot("01_bidata", 0);
    cfg.datos_campos = 3; shot("02_tridata", 0); cfg.datos_campos = 2;
    nav.sog = 40; nav.valid &= (uint16_t)~NV_STW; shot("01b_bidata_0_4_kn", 0);    /* debe verse "0.4", no ".4" */
    nav.valid |= NV_STW;
    key(K_NEXT, KEV_RELEASE); shot("03_viento", 0);
    key(K_NEXT, KEV_RELEASE); shot("04_piloto", 0);
    key(K_NEXT, KEV_RELEASE); act.func = ACT_MOLINETE_BAJAR; act.flags |= AF_RUNNING; shot("05_molinete", 0);
    key(K_NEXT, KEV_RELEASE); act.func = ACT_HELICE_ESTRIBOR; act.run_s = 4; shot("06_helice", 0);
    act.func = 0; act.flags = AF_CHAIN;
    key(K_NEXT, KEV_RELEASE); nav_alive = 0; tick += 10000; shot("07_sin_conexion", 0); nav_alive = 1;
    /* ajustes */
    keys_down_mask = 0;
    cfg.act.paired = 0;
    extern void ui_test_enter_settings(void);
    ui_test_enter_settings(); shot("08_ajustes", 0);
    key(K_2, KEV_PRESS); key(K_6, KEV_RELEASE); shot("09_emparejando", 0);
    strcpy(last_name, "10_bateria_baja"); ui_battery_low_screen();
    printf("pantallas en test/host/out\n");
    printf("interfaz: %d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
