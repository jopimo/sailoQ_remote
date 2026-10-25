/*
 * ui.c - Paginas del mando, teclas y envio de ordenes por radio.
 *
 * Paginas (solo se muestran las de lo que esta emparejado):
 *   base     -> DATOS (bidata/tridata), VIENTO, PILOTO
 *   actuador -> MOLINETE, HELICE DE PROA
 *   siempre  -> AJUSTES (manteniendo PAG< y PAG> 3 s; sale sola si no hay nada emparejado)
 *
 * Teclas (4 filas x 2 columnas):   PAG<  PAG>   |   K1 K2   |   K3 K4   |   K5 K6
 *   PILOTO:   K1 -1, K2 +1, K3 -10, K4 +10, K5 STBY (al momento), K6 AUTO (pulsacion larga)
 *             K1+K3 virada a babor, K2+K4 virada a estribor, K5+K6 modo viento (mantener 0,5 s)
 *   MOLINETE: K1 SUBIR, K2 BAJAR (solo mientras se mantiene: hombre muerto)
 *   HELICE:   K1 BABOR, K2 ESTRIBOR (solo mientras se mantiene)
 *   AJUSTES:  K1 arriba, K2 abajo, K3 -, K4 +, K5 salir, K6 OK (largo = borrar emparejamiento)
 *
 * Sin acentos en los textos: la fuente 6x8 es ASCII.
 */
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "gfx.h"
#include "radio.h"
#include "config.h"
#include "power.h"
#include "buzzer.h"
#include "periph.h"

static uint8_t  page = P_DATOS;
static uint8_t  dirty = 1;
static uint32_t t_draw, t_poll, t_hold;
static uint8_t  combo_used;             /* teclas que ya han actuado: al soltarlas no hacen nada */
static uint8_t  combo_done;             /* combinaciones ya enviadas (hasta soltar las teclas) */
static uint8_t  hold_func;              /* funcion del actuador que se esta manteniendo (ACT_xxx) */
static uint8_t  hold_stop_pending;
static uint8_t  menu_sel;
static uint8_t  pairing;                /* 0 = no, DEV_BASE / DEV_ACTUADOR = buscando */
static uint32_t pair_msg_until;
static char     pair_msg[24];
static uint32_t msg_until;
static char     msg[22];

/* ------------------------------------------------------------------ utilidades */
static void flash_msg(const char *s, uint32_t now, uint32_t ms) { strncpy(msg, s, sizeof msg - 1); msg[sizeof msg - 1] = 0; msg_until = now + ms; dirty = 1; }

static void beep_key(void) { if (cfg.pitido) buz_beep(25); }

/* Numero entero con 'dec' decimales: 1234,1 -> "123.4". Negativos con '-' */
static void fmt_fixed(char *b, int32_t v, int dec)
{
    char t[12]; int n = 0, neg = v < 0;
    uint32_t u = neg ? (uint32_t)(-v) : (uint32_t)v;
    do { t[n++] = (char)('0' + u % 10); u /= 10; if (n == dec) t[n++] = '.'; } while (u || n <= dec);
    if (neg) t[n++] = '-';
    for (int i = 0; i < n; i++) b[i] = t[n - 1 - i];
    b[n] = 0;
}

static int page_ok(uint8_t p)
{
    switch (p) {
    case P_DATOS: case P_VIENTO: case P_PILOTO: return cfg.base.paired;
    case P_MOLINETE: case P_HELICE:             return cfg.act.paired;
    default: return 0;
    }
}

static void goto_page(int dir)
{
    for (int i = 0; i < P_AJUSTES; i++) {
        int p = ((int)page + dir * (i + 1) + P_AJUSTES * 4) % P_AJUSTES;
        if (page_ok((uint8_t)p)) { page = (uint8_t)p; dirty = 1; return; }
    }
    page = P_AJUSTES; dirty = 1;               /* nada emparejado */
}

static int is_base_page(void) { return page == P_DATOS || page == P_VIENTO || page == P_PILOTO; }
static int is_act_page(void)  { return page == P_MOLINETE || page == P_HELICE; }

/* ------------------------------------------------------------------ elementos comunes */
static void draw_header(const char *title, uint32_t now)
{
    gfx_text(3, 2, title, 1, ALIGN_LEFT);
    gfx_invert(0, 0, 128, 12);                 /* texto blanco sobre barra negra */

    /* cobertura de radio (del receptor de la pagina) */
    const peer_state_t *ps = is_act_page() ? &st_act : &st_base;
    int alive = radio_alive(ps, now, 3000);
    int bars = alive ? (ps->link > 80 ? 4 : ps->link > 55 ? 3 : ps->link > 30 ? 2 : 1) : 0;
    for (int i = 0; i < 4; i++) {
        int h = 2 + i * 2;
        if (i < bars) gfx_fill(84 + i * 3, 10 - h, 2, h, 0);
        else gfx_pixel(84 + i * 3, 9, 0);
    }
    /* bateria */
    uint8_t pct = power_vbat_pct();
    int low = power_vbat_mv() < VBAT_LOW_MV && !power_charging();
    if (!low || (now / 500) % 2) {
        gfx_rect(102, 3, 18, 7, 0);
        gfx_fill(120, 5, 2, 3, 0);
        gfx_fill(104, 5, (pct * 14 + 50) / 100, 3, 0);
        if (power_charging()) { gfx_line(108, 2, 111, 6, 1); gfx_line(111, 6, 114, 6, 1); gfx_line(114, 6, 117, 10, 1); }
    }
}

static void draw_dots(uint32_t now)
{
    (void)now;
    int n = 0, k = 0;
    for (int p = 0; p < P_AJUSTES; p++) if (page_ok((uint8_t)p)) { if (p == page) k = n; n++; }
    int x = 64 - n * 7 / 2;
    for (int i = 0; i < n; i++, x += 7) { if (i == k) gfx_fill(x, 123, 4, 4, 1); else gfx_rect(x, 123, 4, 4, 1); }
}

static void draw_nolink(const peer_t *pe, const peer_state_t *ps, uint32_t now)
{
    if (!pe->paired || radio_alive(ps, now, 3000)) return;
    gfx_fill(14, 56, 100, 14, 0);
    gfx_rect(14, 56, 100, 14, 1);
    gfx_text(64, 59, radio_ok() ? "SIN CONEXION" : "SIN RADIO", 1, ALIGN_CENTER);
}

static void draw_hints(const char *l1, const char *r1)
{
    gfx_text(3, 110, l1, 1, ALIGN_LEFT);
    gfx_text(125, 110, r1, 1, ALIGN_RIGHT);
}

/* ------------------------------------------------------------------ paginas */
static void field(int y0, int h, const char *label, const char *unit, int valid, int32_t v, int dec, const font_big_t *f)
{
    char b[12];
    gfx_text(3, y0 + 1, label, 1, ALIGN_LEFT);
    gfx_text(125, y0 + 1, unit, 1, ALIGN_RIGHT);
    if (valid) fmt_fixed(b, v, dec); else strcpy(b, dec ? "--.-" : "--");
    gfx_big(124, y0 + h - f->h - 1, f, b, ALIGN_RIGHT);
}

static void page_datos(uint32_t now)
{
    draw_header(cfg.datos_campos == 3 ? "TRIDATA" : "BIDATA", now);
    int alive = radio_alive(&st_base, now, 5000);
    int vs = alive && (nav.valid & NV_STW), vg = alive && (nav.valid & NV_SOG);
    int32_t vel = vs ? nav.stw : nav.sog;                  /* corredera si la hay; si no, GPS */
    const char *vl = vs ? "VELOCIDAD STW" : "VELOCIDAD SOG";
    int vd = alive && (nav.valid & NV_DEPTH);
    int32_t dep = nav.depth;                               /* 0,1 m */
    if (cfg.datos_campos == 3) {
        field(13, 37, vs ? "VEL STW" : "VEL SOG", "kn", vs || vg, (vel + 5) / 10, 1, &font_big);
        gfx_hline(3, 124, 50, 1);
        field(51, 37, "PROF", "m", vd, dep >= 1000 ? dep / 10 : dep, dep >= 1000 ? 0 : 1, &font_big);
        gfx_hline(3, 124, 88, 1);
        field(89, 33, "AWS", "kn", alive && (nav.valid & NV_AWS), nav.aws, 1, &font_big);
    } else {
        field(13, 54, vl, "kn", vs || vg, (vel + 5) / 10, 1, &font_huge);
        gfx_hline(3, 124, 67, 1);
        field(68, 54, "PROFUNDIDAD", "m", vd, dep >= 1000 ? dep / 10 : dep, dep >= 1000 ? 0 : 1, &font_huge);
    }
    draw_dots(now);
    draw_nolink(&cfg.base, &st_base, now);
}

static const int8_t sin_t[91] = {  /* seno x 100, 0..90 grados */
    0,2,3,5,7,9,10,12,14,16,17,19,21,22,24,26,28,29,31,33,34,36,37,39,41,42,44,45,47,48,50,52,53,54,56,57,59,60,62,63,
    64,66,67,68,69,71,72,73,74,75,77,78,79,80,81,82,83,84,85,86,87,87,88,89,90,91,91,92,93,93,94,95,95,96,96,97,97,97,
    98,98,98,99,99,99,99,100,100,100,100,100,100 };
static int isin(int a) { a %= 360; if (a < 0) a += 360; if (a <= 90) return sin_t[a]; if (a <= 180) return sin_t[180 - a]; if (a <= 270) return -sin_t[a - 180]; return -sin_t[360 - a]; }
static int icos(int a) { return isin(a + 90); }

static void page_viento(uint32_t now)
{
    draw_header("VIENTO", now);
    int alive = radio_alive(&st_base, now, 5000);
    const int cx = 64, cy = 70, r = 44;
    gfx_circle(cx, cy, r, 1); gfx_circle(cx, cy, r - 1, 1);
    for (int a = 0; a < 360; a += 30) {
        int l = (a % 90) ? 4 : 7;
        gfx_line(cx + isin(a) * (r - l) / 100, cy - icos(a) * (r - l) / 100, cx + isin(a) * r / 100, cy - icos(a) * r / 100, 1);
    }
    /* sector de babor (rojo en los instrumentos) y estribor: marcas gruesas a 30-60 grados */
    char b[12];
    if (alive && (nav.valid & NV_AWA)) {
        int a = nav.awa;
        gfx_thick_line(cx, cy, cx + isin(a) * (r - 5) / 100, cy - icos(a) * (r - 5) / 100, 3, 1);
        gfx_disc(cx, cy, 3, 1);
        fmt_fixed(b, a < 0 ? -a : a, 0); strcat(b, a < 0 ? " B" : " E");
    } else strcpy(b, "--");
    gfx_text(3, 15, "AWA", 1, ALIGN_LEFT);
    gfx_text(3, 24, b, 1, ALIGN_LEFT);
    gfx_text(125, 15, "kn", 1, ALIGN_RIGHT);
    if (alive && (nav.valid & NV_AWS)) fmt_fixed(b, nav.aws, 1); else strcpy(b, "--.-");
    gfx_big(cx, cy + 12, &font_med, b, ALIGN_CENTER);
    if (alive && (nav.valid & NV_TWS)) {
        fmt_fixed(b, nav.tws, 1);
        gfx_text(3, 104, "TWS", 1, ALIGN_LEFT);
        gfx_text(3, 113, b, 1, ALIGN_LEFT);
    }
    draw_dots(now);
    draw_nolink(&cfg.base, &st_base, now);
}

static void page_piloto(uint32_t now)
{
    draw_header("PILOTO", now);
    int alive = radio_alive(&st_base, now, 5000);
    static const char *modes[] = { "STANDBY", "AUTO", "VIENTO", "TRACK" };
    const char *m = (alive && (nav.valid & NV_PILOT) && nav.pilot_mode <= PILOT_TRACK) ? modes[nav.pilot_mode] : "----";
    int w = gfx_text_w(m, 1) + 10;
    gfx_text(64, 17, m, 1, ALIGN_CENTER);
    gfx_invert(64 - w / 2, 15, w, 12);

    char b[12];
    int ok = alive && (nav.valid & NV_PILOT) && nav.pilot_mode != PILOT_STBY && nav.pilot_mode != PILOT_NONE;
    gfx_text(64, 32, nav.pilot_mode == PILOT_WIND ? "ANGULO FIJADO" : "RUMBO FIJADO", 1, ALIGN_CENTER);
    if (ok) { fmt_fixed(b, nav.pilot_target % 360, 0); while (strlen(b) < 3) { memmove(b + 1, b, strlen(b) + 1); b[0] = '0'; } strcat(b, "\xB0"); }
    else strcpy(b, "---");
    gfx_big(64, 43, &font_huge, b, ALIGN_CENTER);

    if (alive && (nav.valid & NV_HDG)) { fmt_fixed(b, (nav.hdg + 5) / 10 % 360, 0); }
    else strcpy(b, "---");
    char l[20]; snprintf(l, sizeof l, "RUMBO %s", b);
    gfx_text(64, 89, l, 1, ALIGN_CENTER);
    gfx_text(3, 100, "-1", 1, ALIGN_LEFT);    gfx_text(125, 100, "+1", 1, ALIGN_RIGHT);    /* fila 2 de teclas */
    gfx_text(3, 109, "-10", 1, ALIGN_LEFT);   gfx_text(125, 109, "+10", 1, ALIGN_RIGHT);   /* fila 3 */
    gfx_text(3, 118, "STBY", 1, ALIGN_LEFT);  gfx_text(125, 118, "AUTO", 1, ALIGN_RIGHT);  /* fila 4 */
    draw_dots(now);
    draw_nolink(&cfg.base, &st_base, now);
}

static void act_status_box(int y, const char *running_text, uint32_t now)
{
    const char *s = "PARADO";
    int alive = radio_alive(&st_act, now, 2000);
    if (!alive) s = "--";
    else if (act.flags & AF_BLOCKED) s = "BLOQUEADO";
    else if (act.flags & AF_MAXTIME) s = "TIEMPO MAX";
    else if (act.flags & AF_OVERTEMP) s = "TEMP ALTA";
    else if (act.flags & AF_RUNNING) s = running_text;
    int w = gfx_text_w(s, 1) + 12;
    gfx_text(64, y + 3, s, 1, ALIGN_CENTER);
    gfx_invert(64 - w / 2, y, w, 13);
}

static void page_molinete(uint32_t now)
{
    draw_header("MOLINETE", now);
    char b[12];
    gfx_text(64, 16, "CADENA FUERA", 1, ALIGN_CENTER);
    if (radio_alive(&st_act, now, 3000) && (act.flags & AF_CHAIN)) fmt_fixed(b, (act.chain_dm + 5) / 10, 0); else strcpy(b, "--");
    int w = gfx_big(58, 30, &font_huge, b, ALIGN_CENTER);
    gfx_text(58 + w / 2 + 4, 30 + font_huge.h - 16, "m", 2, ALIGN_LEFT);
    const char *run = act.func == ACT_MOLINETE_SUBIR ? "SUBIENDO" : act.func == ACT_MOLINETE_BAJAR ? "BAJANDO" : "EN MARCHA";
    act_status_box(80, run, now);
    draw_hints("SUBIR", "BAJAR");
    draw_dots(now);
    draw_nolink(&cfg.act, &st_act, now);
}

static void page_helice(uint32_t now)
{
    draw_header("HELICE PROA", now);
    /* barco visto desde arriba, proa arriba */
    const int cx = 64;
    gfx_line(cx, 18, cx - 11, 40, 1); gfx_line(cx, 18, cx + 11, 40, 1);
    gfx_vline(cx - 11, 40, 86, 1); gfx_vline(cx + 11, 40, 86, 1); gfx_hline(cx - 11, cx + 11, 86, 1);
    gfx_fill(cx - 6, 33, 12, 4, 1);                                     /* tunel de la helice */
    int dir = 0;
    if (hold_func == ACT_HELICE_BABOR || act.func == ACT_HELICE_BABOR) dir = -1;
    if (hold_func == ACT_HELICE_ESTRIBOR || act.func == ACT_HELICE_ESTRIBOR) dir = 1;
    if (dir) {
        int x0 = cx + dir * 13, x1 = cx + dir * 46;
        gfx_thick_line(x0, 35, x1, 35, 3, 1);
        gfx_line(x1, 35, x1 - dir * 7, 29, 1); gfx_line(x1, 35, x1 - dir * 7, 41, 1);
        gfx_line(x1 - dir * 7, 29, x1 - dir * 7, 41, 1);
    }
    gfx_text(4, 56, "BABOR", 1, ALIGN_LEFT);
    gfx_text(124, 56, "ESTRIBOR", 1, ALIGN_RIGHT);
    if (dir < 0) gfx_invert(2, 54, 34, 12);
    if (dir > 0) gfx_invert(124 - 50, 54, 52, 12);
    char l[24];
    snprintf(l, sizeof l, "%u s / MAX %u s", radio_alive(&st_act, now, 2000) ? act.run_s : 0, act.max_s ? act.max_s : 30);
    gfx_text(64, 94, l, 1, ALIGN_CENTER);
    draw_hints("BABOR", "ESTRIBOR");
    draw_dots(now);
    draw_nolink(&cfg.act, &st_act, now);
}

/* ------------------------------------------------------------------ ajustes */
enum { M_BASE, M_ACT, M_APAGADO, M_BRILLO, M_CONTRASTE, M_PITIDO, M_CAMPOS, M_INFO, M_N };
static const uint16_t apagados[] = { 15, 30, 60, 120, 300, 600 };

static void menu_value(int i, char *b, size_t n)
{
    switch (i) {
    case M_BASE:      snprintf(b, n, "%s", cfg.base.paired ? cfg.base.name : "--"); break;
    case M_ACT:       snprintf(b, n, "%s", cfg.act.paired ? cfg.act.name : "--"); break;
    case M_APAGADO:   snprintf(b, n, "%u s", cfg.apagado_s); break;
    case M_BRILLO:    if (cfg.brillo == 255) snprintf(b, n, "AUTO"); else snprintf(b, n, "%u %%", cfg.brillo); break;
    case M_CONTRASTE: snprintf(b, n, "%u", cfg.contraste); break;
    case M_PITIDO:    snprintf(b, n, "%s", cfg.pitido ? "SI" : "NO"); break;
    case M_CAMPOS:    snprintf(b, n, "%u", cfg.datos_campos); break;
    default:          snprintf(b, n, "v%s", FW_VERSION); break;
    }
}

static void page_ajustes(uint32_t now)
{
    static const char *names[M_N] = { "BASE", "RECEPTOR", "APAGADO", "BRILLO", "CONTRASTE", "PITIDO", "CAMPOS DATOS", "VERSION" };
    draw_header("AJUSTES", now);
    if (pairing) {
        gfx_text(64, 20, pairing == DEV_BASE ? "BUSCANDO BASE" : "BUSCANDO RECEPTOR", 1, ALIGN_CENTER);
        gfx_text(64, 40, "Pulse el boton de", 1, ALIGN_CENTER);
        gfx_text(64, 50, "emparejar en el", 1, ALIGN_CENTER);
        gfx_text(64, 60, pairing == DEV_BASE ? "equipo base" : "receptor", 1, ALIGN_CENTER);
        int dots = (int)((now / 300) % 4);
        for (int i = 0; i < dots; i++) gfx_disc(55 + i * 9, 78, 2, 1);
        draw_hints("", "K5 CANCELAR");
        return;
    }
    if (now < pair_msg_until) { gfx_text(64, 50, pair_msg, 1, ALIGN_CENTER); return; }
    for (int i = 0; i < M_N; i++) {
        int y = 15 + i * 11;
        char v[16];
        gfx_text(4, y + 1, names[i], 1, ALIGN_LEFT);
        menu_value(i, v, sizeof v);
        gfx_text(124, y + 1, v, 1, ALIGN_RIGHT);
        if (i == menu_sel) gfx_invert(1, y - 1, 126, 11);
    }
    const char *h = "";
    if (menu_sel == M_BASE || menu_sel == M_ACT) h = (menu_sel == M_BASE ? cfg.base.paired : cfg.act.paired) ? "OK largo: BORRAR" : "OK: EMPAREJAR";
    else if (menu_sel < M_INFO) h = "K3 -   K4 +";
    else {
        static char info[24];
        snprintf(info, sizeof info, "%u mV ID %04lX", power_vbat_mv(), (unsigned long)(radio_remote_id() & 0xFFFF));
        h = info;
    }
    gfx_text(64, 106, h, 1, ALIGN_CENTER);
    gfx_text(3, 118, "K5 SALIR", 1, ALIGN_LEFT);
    gfx_text(125, 118, "K6 OK", 1, ALIGN_RIGHT);
}

static void menu_change(int d)
{
    switch (menu_sel) {
    case M_APAGADO: {
        int k = 0; for (int i = 0; i < 6; i++) if (apagados[i] == cfg.apagado_s) k = i;
        k += d; if (k < 0) k = 0; if (k > 5) k = 5; cfg.apagado_s = apagados[k]; break; }
    case M_BRILLO: {
        int v = cfg.brillo == 255 ? -20 : cfg.brillo;   /* AUTO va antes de 0 */
        v += d * 20; if (v < -20) v = -20; if (v > 100) v = 100;
        cfg.brillo = v < 0 ? 255 : (uint8_t)v;
        if (cfg.brillo != 255) bl_set(cfg.brillo);
        break; }
    case M_CONTRASTE: {
        int v = cfg.contraste + d; if (v < 10) v = 10; if (v > 63) v = 63; cfg.contraste = (uint8_t)v; lcd_contrast(cfg.contraste); break; }
    case M_PITIDO: cfg.pitido = !cfg.pitido; break;
    case M_CAMPOS: cfg.datos_campos = cfg.datos_campos == 3 ? 2 : 3; break;
    default: break;
    }
}

static void enter_settings(void) { page = P_AJUSTES; menu_sel = 0; dirty = 1; }
static void leave_settings(void)
{
    cfg_save();
    page = P_DATOS;
    if (!page_ok(page)) goto_page(+1);
    dirty = 1;
}

/* ------------------------------------------------------------------ teclas */
static void pilot(uint8_t code, const char *name, uint32_t now)
{
    beep_key();
    if (radio_pilot_key(code, now) == 0) flash_msg(name, now, 800);
    else { flash_msg("SIN RESPUESTA", now, 1500); buz_beep(300); }
}

void ui_key(const kev_t *e, uint32_t now)
{
    uint8_t k = e->key;
    dirty = 1;

    /* Las teclas que formaron parte de una combinacion no hacen nada al soltarse */
    if (e->type == KEV_RELEASE && (combo_used & KB(k))) { combo_used &= (uint8_t)~KB(k); return; }

    /* Cambio de pagina (al soltar, para permitir la combinacion PAG<+PAG>) */
    if ((k == K_PREV || k == K_NEXT) && page != P_AJUSTES) {
        if (e->type == KEV_RELEASE) { beep_key(); goto_page(k == K_NEXT ? +1 : -1); }
        return;
    }

    switch (page) {
    case P_PILOTO:
        if (k == K_5 && e->type == KEV_PRESS) { pilot(ST_KEY_STBY, "STANDBY", now); combo_used |= KB(K_5); }   /* STBY al momento */
        else if (k == K_6 && e->type == KEV_LONG && !(keys_down() & KB(K_5))) { pilot(ST_KEY_AUTO, "AUTO", now); combo_used |= KB(K_6); }
        else if (e->type == KEV_RELEASE) {
            if (k == K_1) pilot(ST_KEY_M1, "-1", now);
            if (k == K_2) pilot(ST_KEY_P1, "+1", now);
            if (k == K_3) pilot(ST_KEY_M10, "-10", now);
            if (k == K_4) pilot(ST_KEY_P10, "+10", now);
        }
        break;

    case P_MOLINETE:
    case P_HELICE:
        /* el envio continuo se hace en ui_tick(); aqui solo el pitido */
        if ((k == K_1 || k == K_2) && e->type == KEV_PRESS) beep_key();
        if ((k == K_1 || k == K_2) && e->type == KEV_RELEASE) hold_stop_pending = 1;
        break;

    case P_AJUSTES:
        if (pairing) { if (k == K_5 && e->type == KEV_PRESS) { pairing = 0; radio_sleep(); radio_wake(); } break; }
        if (e->type == KEV_PRESS) {
            beep_key();
            if (k == K_PREV || k == K_1) menu_sel = (uint8_t)((menu_sel + M_N - 1) % M_N);
            if (k == K_NEXT || k == K_2) menu_sel = (uint8_t)((menu_sel + 1) % M_N);
            if (k == K_3) menu_change(-1);
            if (k == K_4) menu_change(+1);
            if (k == K_5) leave_settings();
        }
        if (k == K_6 && (menu_sel == M_BASE || menu_sel == M_ACT)) {
            peer_t *pe = menu_sel == M_BASE ? &cfg.base : &cfg.act;
            if (e->type == KEV_LONG && pe->paired) {          /* borrar: pulsacion larga */
                memset(pe, 0, sizeof *pe); cfg_save(); buz_beep(200);
                strcpy(pair_msg, "BORRADO"); pair_msg_until = now + 1200; combo_used |= KB(K_6);
            } else if (e->type == KEV_RELEASE && !pe->paired) {
                pairing = menu_sel == M_BASE ? DEV_BASE : DEV_ACTUADOR;
                radio_pair_start(pairing);
            }
        } else if (k == K_6 && e->type == KEV_PRESS && menu_sel != M_BASE && menu_sel != M_ACT) menu_change(+1);
        break;

    default: break;
    }
}

/* ------------------------------------------------------------------ bucle */
void ui_init(uint32_t now)
{
    (void)now;
    page = P_DATOS;
    if (!page_ok(page)) goto_page(+1);
    if (!cfg.base.paired && !cfg.act.paired) enter_settings();     /* primer arranque: directo a emparejar */
}

void ui_boot_screen(int radio_present)
{
    gfx_clear();
    gfx_text(64, 30, "sailoQ", 2, ALIGN_CENTER);
    gfx_text(64, 50, "remote", 1, ALIGN_CENTER);
    char b[24]; snprintf(b, sizeof b, "v%s", FW_VERSION);
    gfx_text(64, 64, b, 1, ALIGN_CENTER);
    gfx_text(64, 90, radio_present ? "RADIO OK" : "RADIO NO RESPONDE", 1, ALIGN_CENTER);
    snprintf(b, sizeof b, "BATERIA %u mV", power_vbat_mv());
    gfx_text(64, 102, b, 1, ALIGN_CENTER);
    gfx_show();
}

void ui_battery_low_screen(void)
{
    gfx_clear();
    gfx_text(64, 40, "BATERIA", 2, ALIGN_CENTER);
    gfx_text(64, 60, "BAJA", 2, ALIGN_CENTER);
    gfx_text(64, 90, "Cargue el mando", 1, ALIGN_CENTER);
    gfx_show();
}

int ui_busy(void) { return pairing != 0 || hold_func != 0; }

void ui_wake(uint32_t now) { t_poll = 0; dirty = 1; (void)now; }

void ui_tick(uint32_t now)
{
    uint8_t down = keys_down();

    /* PAG< + PAG> durante 3 s -> ajustes */
    if ((down & (KB(K_PREV) | KB(K_NEXT))) == (KB(K_PREV) | KB(K_NEXT)) &&
        keys_held_ms(K_PREV, now) > 3000 && keys_held_ms(K_NEXT, now) > 3000 && page != P_AJUSTES) {
        combo_used |= KB(K_PREV) | KB(K_NEXT);
        buz_beep(150);
        enter_settings();
    }

    /* Combinaciones del piloto (mantener las dos teclas 0,5 s) */
    if (page == P_PILOTO) {
        struct { uint8_t a, b, code; const char *name; } cb[] = {
            { K_1, K_3, ST_KEY_TACK_PORT, "VIRADA BABOR" },
            { K_2, K_4, ST_KEY_TACK_STBD, "VIRADA ESTRIBOR" },
            { K_5, K_6, ST_KEY_WIND, "MODO VIENTO" },
        };
        for (unsigned i = 0; i < 3; i++) {
            uint8_t m = (uint8_t)(KB(cb[i].a) | KB(cb[i].b));
            if ((down & m) != m) { combo_done &= (uint8_t)~m; continue; }
            if (!(combo_done & m) && keys_held_ms(cb[i].a, now) > 500 && keys_held_ms(cb[i].b, now) > 500) {
                combo_done |= m;
                combo_used |= m;
                pilot(cb[i].code, cb[i].name, now);
            }
        }
    }

    /* Actuador: hombre muerto. Solo una tecla a la vez; si se pulsan las dos, PARAR. */
    uint8_t want = ACT_STOP;
    if (is_act_page()) {
        uint8_t k12 = down & (uint8_t)(KB(K_1) | KB(K_2));
        if (k12 == KB(K_1)) want = page == P_MOLINETE ? ACT_MOLINETE_SUBIR : ACT_HELICE_BABOR;
        if (k12 == KB(K_2)) want = page == P_MOLINETE ? ACT_MOLINETE_BAJAR : ACT_HELICE_ESTRIBOR;
    }
    if (want != hold_func) { hold_func = want; t_hold = 0; dirty = 1; if (want == ACT_STOP) hold_stop_pending = 1; }

    /* Radio */
    if (pairing) {
        if (now - t_poll >= 100) {
            t_poll = now;
            int r = radio_pair_step();
            if (r != 0) {
                strcpy(pair_msg, r > 0 ? "EMPAREJADO" : "NO ENCONTRADO");
                if (r > 0) { buz_beep(80); } else buz_beep(400);
                pair_msg_until = now + 1500;
                pairing = 0; dirty = 1;
            }
        }
    } else if (hold_func != ACT_STOP) {
        if (now - t_hold >= PROTO_HOLD_MS) { t_hold = now; radio_act_hold(hold_func, now); }
    } else if (hold_stop_pending && cfg.act.paired) {
        hold_stop_pending = 0;
        radio_act_hold(ACT_STOP, now);           /* el actuador para solo a los 300 ms; esto lo adelanta */
    } else if (is_base_page() && cfg.base.paired) {
        if (now - t_poll >= 250) { t_poll = now; radio_poll_base(page, now); dirty = 1; }
    } else if (is_act_page() && cfg.act.paired) {
        if (now - t_poll >= 400) { t_poll = now; radio_act_poll(now); dirty = 1; }
    }

    /* Dibujo (maximo 10 veces por segundo) */
    if ((dirty || now - t_draw > 500) && now - t_draw >= 100) {
        t_draw = now; dirty = 0;
        gfx_clear();
        switch (page) {
        case P_DATOS:    page_datos(now); break;
        case P_VIENTO:   page_viento(now); break;
        case P_PILOTO:   page_piloto(now); break;
        case P_MOLINETE: page_molinete(now); break;
        case P_HELICE:   page_helice(now); break;
        default:         page_ajustes(now); break;
        }
        if (now < msg_until) {
            int w = gfx_text_w(msg, 1) + 12;
            gfx_fill(64 - w / 2, 56, w, 15, 0); gfx_rect(64 - w / 2, 56, w, 15, 1);
            gfx_text(64, 60, msg, 1, ALIGN_CENTER);
        }
        gfx_show();
    }
}

#ifdef HOST
void ui_test_enter_settings(void) { enter_settings(); }   /* solo para las pruebas en el PC (test/host) */
#endif
