/*
 * radio.c - La radio del mando. El protocolo esta en la libreria sailoQ_radio (sq_mando.h); aqui
 * solo se une con los ajustes del mando (los emparejamientos se guardan en cfg) y se guardan los
 * ultimos datos recibidos para la interfaz.
 */
#include "radio.h"
#include "config.h"

pkt_nav_t        nav;
pkt_act_status_t act;
peer_state_t     st_base, st_act;

static uint8_t   pair_dev;

int      radio_init(void)      { return sqm_init(); }
int      radio_ok(void)        { return sqm_ok(); }
uint32_t radio_remote_id(void) { return sqm_id(); }
void     radio_wake(void)      { sqm_wake(); }
void     radio_sleep(void)     { sqm_sleep(); }

/* 'now' es HAL_GetTick(), el mismo reloj que usa la libreria (sq_port_ms) */
int radio_poll_base(uint8_t page, uint32_t now) { (void)now; return sqm_poll_base(&cfg.base, &st_base, page, &nav); }
int radio_pilot_key(uint8_t key, uint32_t now)  { (void)now; return sqm_pilot_key(&cfg.base, &st_base, key, &nav); }
int radio_act_hold(uint8_t func, uint32_t now)  { (void)now; return sqm_act_hold(&cfg.act, &st_act, func, &act); }
int radio_act_poll(uint32_t now)                { (void)now; return sqm_act_poll(&cfg.act, &st_act, &act); }

int radio_alive(const peer_state_t *p, uint32_t now, uint32_t ms) { return p->last_ok_ms && now - p->last_ok_ms < ms; }

void radio_pair_start(uint8_t dev_type)
{
    pair_dev = dev_type;
    sqm_pair_start(dev_type);
}

int radio_pair_step(void)
{
    sq_peer_t p;
    int r = sqm_pair_step(&p);
    if (r == 1) {
        *(pair_dev == DEV_BASE ? &cfg.base : &cfg.act) = p;
        cfg_save();
    }
    return r;
}
